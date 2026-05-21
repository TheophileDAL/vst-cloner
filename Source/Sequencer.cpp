//
//  Sequencer.cpp
//  vst_cloner - ConsoleApp
//
//  Created by Théophile Dal on 05/03/2026.
//

#include "Sequencer.h"

Sequencer::Sequencer(int chanel, int lower_note, int higher_note, std::vector<int> velocities) : chanel(chanel){
    for (int note = lower_note; note < higher_note + 1; ++note)
        for (int i = 0; i < velocities.size(); ++i){
            notes_sequence.push({note,velocities[i]});
        }
    keyPressed = false;
    state = "askingForRecorder";
}
    
std::array<int,2> Sequencer::playNote(){
    start_time_note = std::chrono::high_resolution_clock::now();
    keyPressed = true;
    return notes_sequence.front();
}
    
int Sequencer::stopNote(){
    int note = notes_sequence.front()[0];
    notes_sequence.pop();
    keyPressed = false;
    return note;
}

std::string Sequencer::getState(){
    if (notes_sequence.empty()){
        //end this sequencer
        state = "done";
    }
    return state;
}

void Sequencer::setState(std::string state){
    this->state = state;
}
    
long long Sequencer::getDelaySinceNotePlayed()
{
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_note).count();
}
