#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <SD.h>
#include "driver/i2s.h"

// Pin and buffer configuration constants
#define SD_CS          5
#define I2S_DOUT       22
#define I2S_BCLK       26
#define I2S_LRC        25
#define I2S_NUM        I2S_NUM_0 
#define NUM_BYTES_TO_READ_FROM_FILE 1024

// WAV Header struct definition
struct WavHeader_Struct {
    char RIFFSectionID[4];
    uint32_t Size;
    char RiffFormat[4];
    char FormatSectionID[4];
    uint32_t FormatSize;
    uint16_t FormatID;
    uint16_t NumChannels;
    uint32_t SampleRate;
    uint32_t ByteRate;
    uint16_t BlockAlign;
    uint16_t BitsPerSample;
    char DataSectionID[4];
    uint32_t DataSize;
};

// Extern variables
extern File WavFile;
extern struct WavHeader_Struct WavHeader;

// Function prototypes for audio control
void setupAudio();                 // Initializes audio setup
void loopAudio();                  // Audio playback loop
void PlayWav();                    // Manages WAV file playback

// Functions for handling WAV file data
uint16_t ReadFile(byte* Samples);  // Reads data from WAV file
bool FillI2SBuffer(byte* Samples, uint16_t BytesInBuffer); // Fills I2S buffer with audio data

// Utility functions
void SDCardInit();                 // Initializes SD card
bool ValidWavData(struct WavHeader_Struct* Wav); // Validates WAV header
void DumpWAVHeader(struct WavHeader_Struct* Wav); // Prints WAV header info
void PrintData(const char* Data, uint8_t NumBytes); // Utility for printing header data

// Declare the missing functions here
void playCurrentFile();            // Plays the current audio file
void nextFile();                   // Moves to the next audio file

#endif // AUDIOPLAYER_H





// #ifndef AUDIOPLAYER_H
// #define AUDIOPLAYER_H

// #include <SD.h>
// #include "driver/i2s.h"

// #define SD_CS          5
// #define I2S_DOUT      22
// #define I2S_BCLK      26
// #define I2S_LRC       25
// #define I2S_NUM       I2S_NUM_0 
// #define NUM_BYTES_TO_READ_FROM_FILE 1024

// // Complete definition of the struct
// struct WavHeader_Struct {
//     char RIFFSectionID[4];
//     uint32_t Size;
//     char RiffFormat[4];
//     char FormatSectionID[4];
//     uint32_t FormatSize;
//     uint16_t FormatID;
//     uint16_t NumChannels;
//     uint32_t SampleRate;
//     uint32_t ByteRate;
//     uint16_t BlockAlign;
//     uint16_t BitsPerSample;
//     char DataSectionID[4];
//     uint32_t DataSize;
// };

// extern File WavFile;
// extern struct WavHeader_Struct WavHeader;

// void setupAudio();
// void loopAudio();
// void PlayWav();
// uint16_t ReadFile(byte* Samples);
// bool FillI2SBuffer(byte* Samples, uint16_t BytesInBuffer);
// void SDCardInit();
// bool ValidWavData(struct WavHeader_Struct* Wav);
// void DumpWAVHeader(struct WavHeader_Struct* Wav);
// void PrintData(const char* Data, uint8_t NumBytes);

// #endif // AUDIOPLAYER_H
