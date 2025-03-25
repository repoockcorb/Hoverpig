#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

// Include Arduino core
#include <Arduino.h>
#include "DFRobotDFPlayerMini.h"

// Function prototypes
void setupAudio();
void playFile(uint8_t fileNumber);
void updateAudio();
uint16_t getTotalSoundCount();

// External variables
extern bool isPlaying;
extern DFRobotDFPlayerMini dfPlayer;

#endif // AUDIO_PLAYER_H
