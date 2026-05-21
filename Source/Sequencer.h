//
//  Sequencer.h
//  vst_cloner - ConsoleApp
//
//  Created by Théophile Dal on 05/03/2026.
//

#include <queue>
#include <array>
#include <iostream>
#include <chrono>
#include <JuceHeader.h>

class Sequencer{
public:
    Sequencer(int chanel, int lower_note, int higher_note, std::vector<int> velocities);
    
    std::array<int,2> playNote();
    
    int stopNote();
    
    int actionToProcess();
    
    long long getDelaySinceNotePlayed();
    
    std::string getState();
    
    void setState(std::string state);
    
private:
    std::queue<std::array<int,2>> notes_sequence;
    int chanel;
    long long duration = 1000;
    std::chrono::high_resolution_clock::time_point start_time_note;
    bool keyPressed;
    std::string state;
};
