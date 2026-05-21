//
//  Main.cpp
//  vst_cloner - ConsoleApp
//
//  Created by Théophile Dal on 05/03/2026.
//

#include <JuceHeader.h>
#include <string>
#include <Generator.cpp>

int main()
{
    juce::ConsoleApplication app;
    juce::ScopedJuceInitialiser_GUI juceInit;
    
    Generator generator(8, 8, "WAVS");
    generator.ProcessAllMidi();
    
    return 0;
}
