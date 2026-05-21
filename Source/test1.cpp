#include <JuceHeader.h>
#include <iostream>

int main1()
{
    // Ouvre le port MIDI virtuel
    juce::ScopedJuceInitialiser_GUI libraryInitialiser;
    
    /*auto devices = juce::MidiOutput::getAvailableDevices();
    std::cout << "Ports MIDI disponibles :" << std::endl;
    for (auto& device : devices)
        std::cout << "  " << device.name.toStdString() << std::endl;
    
    auto midiOut = juce::MidiOutput::openDevice("IAC Bus 1");
     
    if (!midiOut)
    {
     std::cout << "Impossible d'ouvrir le port MIDI !" << std::endl;
     return 1;
    }*/
    
    auto midiOut = juce::MidiOutput::createNewDevice("KontaktVirtualOut");
        if (!midiOut)
        {
            std::cout << "Impossible de créer le port virtuel !" << std::endl;
            return 1;
        }
    
    std::cout << "Port MIDI ouvert !" << std::endl;
    
    std::cout << "Appuyez sur entrée pour continuer...";
    std::cin.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );

    // Envoie un Note On (C4, velocity 100) sur le canal 1
    midiOut->sendMessageNow(juce::MidiMessage::noteOn(1, 21, (juce::uint8)127));
    std::cout << "Note On envoyée" << std::endl;

    // Attend 1 seconde
    juce::Thread::sleep(10000);

    // Envoie le Note Off
    midiOut->sendMessageNow(juce::MidiMessage::noteOff(1, 21));
    std::cout << "Note Off envoyée" << std::endl;

    return 0;
}
