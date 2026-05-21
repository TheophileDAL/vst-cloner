#include <JuceHeader.h>
#include <atomic>

class WavRecordingCallback : public juce::AudioIODeviceCallback
{
public:
    WavRecordingCallback(juce::File outputFile, double sampleRate, int numChannels, int bitsPerSample)
    {
        juce::WavAudioFormat wavFormat;

        // Create the output stream
        auto outStream = outputFile.createOutputStream();
        if (!outStream)
        {
            DBG("Failed to create output stream for WAV file");
            return;
        }

        // Prepare writer options (modern API)
        writer.reset(
            wavFormat.createWriterFor(outStream.release(), (unsigned int) sampleRate, (unsigned int) numChannels, bitsPerSample, {}, 0)
        );

        if (!writer)
        {
            DBG("Failed to create AudioFormatWriter");
            return;
        }

        threadedWriter.reset(new juce::AudioFormatWriter::ThreadedWriter(writer.get(), backgroundThread, 32768));
        backgroundThread.startThread();
    }

    ~WavRecordingCallback() override
    {
        backgroundThread.stopThread(2000);
        threadedWriter.reset();
        writer.reset();
    }

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        const double deviceRate = device->getCurrentSampleRate();
        const int deviceBlockSize = device->getCurrentBufferSizeSamples();
        const int deviceInChans = device->getActiveInputChannels().countNumberOfSetBits();
        const int deviceOutChans = device->getActiveOutputChannels().countNumberOfSetBits();

        juce::ignoreUnused(deviceRate, deviceBlockSize, deviceOutChans);

        tempBuffer.setSize(juce::jmax(1, deviceInChans), deviceBlockSize);
    }

    void audioDeviceStopped() override
    {
        tempBuffer.setSize(0, 0);
    }

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override
    {
        juce::ignoreUnused(context);

        // Clear outputs (pass-through not needed for recording)
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);

        if (threadedWriter == nullptr)
            return;

        // Copy inputs to temp buffer
        tempBuffer.setSize(juce::jmax(1, numInputChannels), numSamples, false, false, true);

        for (int ch = 0; ch < numInputChannels; ++ch)
            if (inputChannelData[ch] != nullptr)
                tempBuffer.copyFrom(ch, 0, inputChannelData[ch], numSamples);

        // If fewer channels than expected, fill remaining with zeros
        for (int ch = numInputChannels; ch < tempBuffer.getNumChannels(); ++ch)
            tempBuffer.clear(ch, 0, numSamples);

        // Interleave and push to writer
        juce::AudioBuffer<float> interleaveView = tempBuffer; // writer accepts planar via write method below
        threadedWriter->write(interleaveView.getArrayOfReadPointers(), numSamples);
    }

private:
    juce::TimeSliceThread backgroundThread { "WAV Writer Thread" };
    juce::AudioBuffer<float> tempBuffer;

    std::unique_ptr<juce::AudioFormatWriter> writer;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
};

int main4()
{
    juce::ConsoleApplication app;

    // Configure output path
    juce::File outFile = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                            .getChildFile("RecordedVirtualInput.wav");

    // Audio device manager: we want input enabled
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::AudioDeviceManager deviceManager;
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    setup.useDefaultInputChannels = true;   // enable default input
    setup.useDefaultOutputChannels = false; // no need for output
    deviceManager.initialise(16, 0, nullptr, true, {}, nullptr);

    // Optional: select a specific virtual audio device by name
    auto* types = deviceManager.getAvailableDeviceTypes()[0];
    types->scanForDevices();
    auto names = types->getDeviceNames(true); // true -> input names
    for (auto& n : names) DBG(n);
    setup.inputDeviceName = "Pro Tools Audio Bridge 16";
    deviceManager.setAudioDeviceSetup(setup, true);

    // Create the recording callback using the device's sample rate and channel layout
    auto* device = deviceManager.getCurrentAudioDevice();
    if (device == nullptr)
    {
        DBG("No audio device available.");
        return 1;
    }

    const double sampleRate = device->getCurrentSampleRate() > 0 ? device->getCurrentSampleRate() : 48000.0;
    const int numInputChannels = juce::jmax(1, device->getActiveInputChannels().countNumberOfSetBits());
    const int bitsPerSample = 24;

    WavRecordingCallback recorder(outFile, sampleRate, numInputChannels, bitsPerSample);
    deviceManager.addAudioCallback(&recorder);

    // Record for a fixed duration (e.g., 10 seconds)
    const int durationSeconds = 10;
    juce::Thread::sleep(durationSeconds * 1000);

    deviceManager.removeAudioCallback(&recorder);

    DBG("Recording finished: " + outFile.getFullPathName());
    return 0;
}
