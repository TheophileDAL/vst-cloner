//
//  WavRecordingCallback.cpp
//  vst_cloner - ConsoleApp
//
//  Created by Théophile Dal on 06/03/2026.
//
#include "WavRecordingCallback.h"

WavRecordingCallback::WavRecordingCallback(double sr, int nc, int bps) : sampleRate(sr), numChannels(nc), bitsPerSample(bps)
{
    writerThread.startThread();
}

WavRecordingCallback::~WavRecordingCallback()
{
    std::lock_guard<std::mutex> lock(waitingMutex);

    for (auto &b : writers) {
        if (b.threaded) {
            b.threaded.reset();
        }
    }

    writerThread.stopThread(2000);

    for (auto &b : writers) {
        if (b.writer) {
            b.writer.release();
            b.writer.reset();
        }
    }
    writers.clear();
    tempBuffer.setSize(0, 0);
}

void WavRecordingCallback::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    const double deviceRate = device->getCurrentSampleRate();
    const int deviceBlockSize = device->getCurrentBufferSizeSamples();
    const int deviceInChans = device->getActiveInputChannels().countNumberOfSetBits();
    const int deviceOutChans = device->getActiveOutputChannels().countNumberOfSetBits();

    juce::ignoreUnused(deviceRate, deviceBlockSize, deviceOutChans);

    tempBuffer.setSize(juce::jmax(1, deviceInChans), deviceBlockSize);
}
    
void WavRecordingCallback::addWriters(uint8_t chanel_nb) {
    
    for(uint8_t i=0; i<chanel_nb; ++i){
            WriterBundle b;
            b.chanel = i * 2;
            writers.push_back(std::move(b));
    }
}
    
void WavRecordingCallback::deleteWriter(uint8_t chanel) {
    // Stop and remove the writer matching the given channel
    auto it = std::find_if(writers.begin(), writers.end(),
        [chanel](const WriterBundle& b){ return b.chanel == chanel; });

    if (it != writers.end())
    {
        it->threaded.reset();
        it->writer.release();
        it->writer.reset();
        writers.erase(it);
    }
}

    
void WavRecordingCallback::setWriter(uint8_t chanel, float fundamental, juce::File file){
    juce::WavAudioFormat wavFormat;
            
    auto it = std::find_if(writers.begin(), writers.end(),
        [chanel](const WriterBundle& b){ return b.chanel == chanel;});

    if (auto out = file.createOutputStream()) {
        auto writer = std::unique_ptr<juce::AudioFormatWriter>(
            wavFormat.createWriterFor(out.release(), sampleRate, 2, bitsPerSample, {}, 0)
        );
        if (writer) {
            std::lock_guard<std::mutex> lock(waitingMutex);
            it->writer.release();
            it->writer = std::move(writer);
            it->threaded.reset(new juce::AudioFormatWriter::ThreadedWriter(it->writer.get(), writerThread, 32768));
            it->state = "starting";
            it->Af = aWeighting(fundamental);
            it->dbA = 0.0;
        }
    }
}


void WavRecordingCallback::audioDeviceStopped(){
    tempBuffer.setSize(0, 0);
}

void WavRecordingCallback::setWriterState(uint8_t chanel, std::string state){
    std::lock_guard<std::mutex> lock(waitingMutex);
    auto it = std::find_if(writers.begin(), writers.end(),
        [chanel](const WriterBundle& b){ return b.chanel == chanel; });
    it->state = state;
}

std::string WavRecordingCallback::getWriterState(uint8_t chanel){
    std::lock_guard<std::mutex> lock(waitingMutex);
    auto it = std::find_if(writers.begin(), writers.end(),
        [chanel](const WriterBundle& b){ return b.chanel == chanel; });
    return it->state;
}

float WavRecordingCallback::getWriterDbA(uint8_t chanel){
    std::lock_guard<std::mutex> lock(waitingMutex);
    auto it = std::find_if(writers.begin(), writers.end(),
        [chanel](const WriterBundle& b){ return b.chanel == chanel; });
    return it->dbA;
}

float WavRecordingCallback::aWeighting(float frequence)
{
    double f2 = frequence * frequence;

    double num = pow(12194.0,2) * pow(frequence,4);
    double den = (f2 + pow(20.6,2)) *
                 sqrt((f2 + pow(107.7,2)) *
                      (f2 + pow(737.9,2))) *
                 (f2 + pow(12194.0,2));

    return 2.0 + 20.0 * log10(num / den);
}

void WavRecordingCallback::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                      int numInputChannels,
                                      float* const* outputChannelData,
                                      int numOutputChannels,
                                      int numSamples,
                                      const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(context);

    // Clear outputs (pass-through not needed for recording)
    for (uint8_t ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);

    // Copy inputs to temp buffer
    tempBuffer.setSize(juce::jmax(1, numInputChannels), numSamples, false, false, true);

    for (uint8_t ch = 0; ch < numInputChannels; ++ch)
        if (inputChannelData[ch] != nullptr)
            tempBuffer.copyFrom(ch, 0, inputChannelData[ch], numSamples);

    // If fewer channels than expected, fill remaining with zeros
    for (uint8_t ch = numInputChannels; ch < tempBuffer.getNumChannels(); ++ch)
        tempBuffer.clear(ch, 0, numSamples);
    
    tempBuffer.applyGain(10.0f);

    // Interleave and push to writers
    juce::AudioBuffer<float> interleaveView = tempBuffer;
    
    std::lock_guard<std::mutex> lock(waitingMutex);
        
    for (auto &b : writers) {
        if (b.threaded) {
            if (b.state == "writing" and b.chanel + 1 <= numInputChannels){
                float rms = (tempBuffer.getRMSLevel(b.chanel, 0, numSamples) + tempBuffer.getRMSLevel(b.chanel + 1, 0, numSamples)) * 0.5f;
                float db = juce::Decibels::gainToDecibels(rms);
                //b.dbA = db + b.Af + 98.5;
                b.dbA = db + b.Af + 110;
                if (b.dbA > 0){
                    const float* left  = tempBuffer.getReadPointer(b.chanel);
                    const float* right = tempBuffer.getReadPointer(b.chanel + 1);
                    const float* stereo[2] = { left, right };
                    b.threaded->write(stereo, numSamples);
                }
            }
            else if (b.state == "stoping"){
                b.threaded.reset();
                b.state = "stopped";
            }
            else if (b.state == "starting"){
                b.state = "writing";
            }
        }
    }
}

