// WavRecordingCallback.h
//  vst_cloner - ConsoleApp
//
//  Created by Théophile Dal on 06/03/2026.
//

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <mutex>
#include <iostream>
#include <string>


class WavRecordingCallback : public juce::AudioIODeviceCallback
{
public:
    WavRecordingCallback(double sr, int nc, int bps);
    ~WavRecordingCallback() override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

    // Ajoute un writer
    void addWriters(uint8_t channel);

    // Stoper puis Supprimer un writer existant
    void deleteWriter(uint8_t channel);
    
    void setWriter(uint8_t channel, float fundamental, juce::File file);
    
    float getWriterDbA(uint8_t chanel);
    
    std::string getWriterState(uint8_t chanel);
    
    void setWriterState(uint8_t chanel, std::string);
    
    float aWeighting(float frequence);

private:
    
    struct WriterBundle
    {
        std::unique_ptr<juce::AudioFormatWriter> writer;
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threaded;
        uint8_t chanel;
        std::string state;
        float Af;
        float dbA;
    };
    
    // Writers actifs
    std::vector<WriterBundle> writers;

    // Thread pour exécuter tous les ThreadedWriter
    juce::TimeSliceThread writerThread { "WAV Writers" };

    // Buffer temporaire pour copier les entrées audio
    juce::AudioBuffer<float> tempBuffer;

    // Propriétés de l'audio
    double sampleRate;
    int numChannels;
    int bitsPerSample;

    // Mutex pour protéger waiting_writers
    std::mutex waitingMutex;
    
    float lowest_rms;
};
