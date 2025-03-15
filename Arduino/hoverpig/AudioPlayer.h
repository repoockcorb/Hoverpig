#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

// Include Arduino core
#include <Arduino.h>
#include "DFRobotDFPlayerMini.h"

// Pin configuration for DFPlayer Mini
#define PIN_MP3_RX 5  // Connects to module's TX
#define PIN_MP3_TX 4  // Connects to module's RX

// Function prototypes for audio control
void setupAudio();                 // Initializes audio setup
void loopAudio();                  // Audio playback loop
void playRandomFile();             // Plays a random file from the SD card
void playCurrentFile();            // Plays the current file
void stopPlayback();               // Stops audio playback
void resumePlayback();             // Resumes audio playback
void nextFile();                   // Moves to the next audio file
void setVolume(uint8_t volume);    // Sets the volume (0-30)
void checkSDCard();                // Checks SD card status and files

// External declarations
extern HardwareSerial audioSerial;
extern DFRobotDFPlayerMini audioPlayer;
extern int currentFileIndex;
extern const int numFiles;
extern bool isPlaying;             // Track if audio is currently playing

#endif // AUDIOPLAYER_H
