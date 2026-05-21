/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include <JuceHeader.h>
#include <iostream>

int main3()
{
    juce::ScopedJuceInitialiser_GUI libraryInitialiser;
    
    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats(); // AU + VST3 si activé

    juce::PluginDescription desc;
    desc.name = "Kontakt";
    desc.pluginFormatName = "AudioUnit"; // AU
    desc.fileOrIdentifier = "/Library/Audio/Plug-Ins/Components/Kontakt 8.component";

    juce::String error;
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    plugin = formatManager.createPluginInstance(desc, 48000, 512, error);

    if (!plugin)
    {
        std::cout << "Erreur plugin : " << error << std::endl;
        return 1;
    }
    std::cout << "Plugin chargé !" << std::endl;
    
    plugin->prepareToPlay(48000, 512); // sample rate et block size
    
    juce::File presetFile("/Users/Shared/Claire Library/Snapshots/Claire/Basic Claire.nksn"); // ton .nki préparé
    if (!presetFile.exists())
    {
        std::cout << "Preset .nki introuvable !" << std::endl;
    }
    else
    {
        juce::MemoryBlock presetData;
        presetFile.loadFileAsData(presetData);
        plugin->setStateInformation(presetData.getData(), presetData.getSize());
    }
    
    const double sampleRate = 48000;
    const int durationInSeconds = 10;
    const int totalSamples = (int)(sampleRate * durationInSeconds);
    const int blockSize = 512;

    juce::AudioBuffer<float> audioBuffer(2, totalSamples);
    audioBuffer.clear();
    juce::MidiBuffer midi;

    // Note on à sample 0
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    // Note off à la fin
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), totalSamples);
    
    int numBlocks = (totalSamples + blockSize - 1) / blockSize;

    for (int i = 0; i < numBlocks; ++i)
    {
        int startSample = i * blockSize;
        int thisBlockSize = std::min(blockSize, totalSamples - startSample);

        juce::AudioBuffer<float> blockBuffer(audioBuffer.getNumChannels(), thisBlockSize);
        blockBuffer.clear();

        // Crée un bloc MIDI pour ce morceau
        juce::MidiBuffer blockMidi;

        for (auto m : midi)
        {
            auto msg = m.getMessage();
            int samplePos = m.samplePosition - startSample;
            if (m.samplePosition >= startSample && m.samplePosition < startSample + thisBlockSize)
            {
                blockMidi.addEvent(msg, samplePos);
            }
        }

        plugin->processBlock(blockBuffer, blockMidi);

        for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
            audioBuffer.copyFrom(ch, startSample, blockBuffer, ch, 0, thisBlockSize);
    }
    
    juce::File outFile("/Users/theophiledal/Music/VST/C4.wav");
    juce::WavAudioFormat wavFormat;
    auto outStream = outFile.createOutputStream();

    if (outStream)
    {
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(outStream.release(), 48000, 2, 24, {}, 0)
        );

        if (writer)
        {
            writer->writeFromAudioSampleBuffer(audioBuffer, 0, audioBuffer.getNumSamples());
            std::cout << "WAV exporté !" << std::endl;
        } // writer détruit automatiquement ici
    }
    
    plugin->releaseResources();
    plugin.reset();
}
