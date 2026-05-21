//
//  Generator.cpp
//  vst_cloner - ConsoleApp
//
//  Created by Théophile Dal on 05/03/2026.
//

#include <JuceHeader.h>
#include <string>
#include "Sequencer.h"
#include "WavRecordingCallback.h"
#include "dict_midi_key.h"

class Generator{
public:
    Generator(int sequencer_nb, int velocity_nb, std::string directory_name) : sequencer_nb(sequencer_nb), endProcessSequencers(sequencer_nb, false), directory("/Users/theophiledal/PROJETS/MUSIQUE/VST_CLONER/" + directory_name){
        
        //MIDI
        midiOut = juce::MidiOutput::createNewDevice("KontaktVirtualOut");
        if (!midiOut)
        {
            std::cout << "Impossible de créer le port virtuel !" << std::endl;
        }
        else
        {
            std::cout << "Port MIDI ouvert !" << std::endl;
            
            std::cout << "Appuyez sur entrée pour continuer...";
            std::cin.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );
        }
        
        
        std::vector<int> all_velocities(velocity_nb);
        std::iota(all_velocities.begin(), all_velocities.end(), 1);
        auto step = 127.0 / velocity_nb;
        std::transform(all_velocities.begin(), all_velocities.end(),all_velocities.begin(),[step](int i){ return int(i * step); });
        std::vector<int>::iterator it_begin = all_velocities.begin();
        std::vector<int>::iterator it_end;
        std::vector<int> seq_velocities;
                
        for(uint8_t i=0; i<sequencer_nb; ++i){
            if (i < velocity_nb % sequencer_nb){
                seq_velocities.resize(int(velocity_nb/sequencer_nb) + 1);
                it_end = it_begin + int(velocity_nb/sequencer_nb) + 1;
            }
            else{
                seq_velocities.resize(int(velocity_nb/sequencer_nb));
                it_end = it_begin + int(velocity_nb/sequencer_nb);
            }
            
            std::copy(it_begin, it_end, seq_velocities.begin());
            sequencers.push_back(std::make_unique<Sequencer>(i + 1, 21, 108, seq_velocities));
            
            it_begin = it_end;
        }
        
        //AUDIO
        setup.useDefaultInputChannels = true;   // enable default input
        setup.useDefaultOutputChannels = false; // no need for output
        setup.bufferSize = 256;
        deviceManager.initialise(sequencer_nb*2, 0, nullptr, true, {}, nullptr);

        auto* types = deviceManager.getAvailableDeviceTypes()[0];
        types->scanForDevices();
        auto names = types->getDeviceNames(true); // true -> input names
        for (auto& n : names) DBG(n);
        setup.inputDeviceName = "Pro Tools Audio Bridge 16";
        deviceManager.setAudioDeviceSetup(setup, true);

        // Create the recording callback using the device's sample rate and channel layout
        auto* device = deviceManager.getCurrentAudioDevice();
        if (device == nullptr){
            DBG("No audio device available.");
        }
        else{
            const double sampleRate = device->getCurrentSampleRate() > 0 ? device->getCurrentSampleRate() : 48000.0;
            const int numInputChannels = juce::jmax(1, device->getActiveInputChannels().countNumberOfSetBits());
            const int bitsPerSample = 24;

            recorder = std::make_unique<WavRecordingCallback>(sampleRate,numInputChannels,bitsPerSample);
            deviceManager.addAudioCallback(recorder.get());
            
            recorder->addWriters(sequencer_nb);

        }
    }
    
    void SendMidi(int sequencer_num){
        std::array<int,2> send_midi = sequencers[sequencer_num].get()->playNote();
        midiOut->sendMessageNow(juce::MidiMessage::noteOn(sequencer_num + 1, send_midi[0], (juce::uint8)send_midi[1]));
    }
    
    void StopMidi(int sequencer_num){
        int note = sequencers[sequencer_num].get()->stopNote();
        midiOut->sendMessageNow(juce::MidiMessage::noteOff(sequencer_num + 1, note));
    }
    
    bool endProcessGenerator() {
        return std::all_of(endProcessSequencers.begin(), endProcessSequencers.end(), [](bool v){ return v; });
    }
    
    float midiToFreq(int midiNote)
    {
        return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
    }
        
    void ProcessAllMidi(){
        while (!endProcessGenerator()){
            for(int i=0; i<sequencer_nb; ++i){
                if (!endProcessSequencers[i]){
                    std::string state = sequencers[i]->getState();
                    if (state == "askingForRecorder"){
                        std::array<int,2> send_midi = sequencers[i]->playNote();
                        // Configure output path
                        juce::File outFile = directory.getChildFile(midiToNote.at(send_midi[0]) + "_V" + std::to_string(send_midi[1]) + ".wav");
                        recorder->setWriter(i*2, midiToFreq(send_midi[0]), outFile);
                        sequencers[i]->setState("waitingForRecorder");
                    }
                    else if (state == "waitingForRecorder"){
                        if (recorder->getWriterState(i*2) == "writing"){
                            SendMidi(i);
                            std::cout << i << " : SendMidi" << std::endl;
                            sequencers[i]->setState("playing");
                        }
                        else if (recorder->getWriterState(i*2) == "stopped"){
                            sequencers[i]->setState("askingForRecorder");
                        }
                    }
                    else if (state == "playing"){
                        if (recorder->getWriterDbA(i*2) < 0 and sequencers[i]->getDelaySinceNotePlayed() > 1000){
                            recorder->setWriterState(i*2, "stoping");
                            StopMidi(i);
                            std::cout << i << " : StopMidi" << std::endl;
                            sequencers[i]->setState("waitingForRecorder");
                        }
                    }
                    else{
                        recorder->deleteWriter(i*2);
                        std::cout << i << " : endProcessSequencers" << std::endl;
                        endProcessSequencers[i] = true;
                    }
                }
            }
        }
    }
    
private:
    //MIDI
    std::unique_ptr<juce::MidiOutput> midiOut;
    std::vector<std::unique_ptr<Sequencer>> sequencers;
    int sequencer_nb;
    std::vector<bool> endProcessSequencers;
    
    //AUDIO
    juce::AudioDeviceManager deviceManager;
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    std::unique_ptr<WavRecordingCallback> recorder;
    float lowest_rms;

    juce::File directory;
};


int main()
{
    juce::ConsoleApplication app;
    juce::ScopedJuceInitialiser_GUI juceInit;
    
    Generator generator(8, 8, "WAVS");
    generator.ProcessAllMidi();
    
    return 0;
}
