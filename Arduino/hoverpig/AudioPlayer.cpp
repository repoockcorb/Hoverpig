#include "AudioPlayer.h"

// Define pins
const int RX_PIN = 32;   // Connect to TX on DFPlayer
const int TX_PIN = 33;   // Connect to RX on DFPlayer

DFRobotDFPlayerMini dfPlayer;
HardwareSerial audioSerial(1);  // Use hardware serial 1

// Sound states
bool isPlaying = false;
unsigned long soundStartTime = 0;
const unsigned long SOUND_DURATION = 3000;  // Maximum sound duration in milliseconds
uint16_t totalSoundCount = 0;  // Store the total number of sound files

void setupAudio() {
  // Initialize serial for DFPlayer
  audioSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  
  Serial.println("Initializing DFPlayer...");
  
  // Initialize DFPlayer
  if (!dfPlayer.begin(audioSerial)) {
    Serial.println("Could not initialize DFPlayer. Check connections and SD card.");
  } else {
    Serial.println("DFPlayer initialized!");
    
    // Set volume (0-30)
    dfPlayer.volume(25);
    delay(100);
    
    // Get and store the total number of files on the SD card
    totalSoundCount = dfPlayer.readFileCounts();
    
    // Print the number of files on the SD card
    Serial.print("Number of files on SD card: ");
    Serial.println(totalSoundCount);
  }
}

void playFile(uint8_t fileNumber) {
  // Debug output
  Serial.print("Playing sound file: ");
  Serial.println(fileNumber);
  
  // Only play if not already playing
  if (!isPlaying) {
    // Safety check for valid file numbers (1-255)
    if (fileNumber > 0 && fileNumber <= 255) {
      // Play the file and set the isPlaying flag
      dfPlayer.play(fileNumber);
      isPlaying = true;
      soundStartTime = millis();
      
      Serial.print("Started playing file: ");
      Serial.println(fileNumber);
    } else {
      Serial.println("Invalid file number, must be between 1 and 255");
    }
  } else {
    Serial.println("Already playing a sound, ignoring request");
  }
}

void updateAudio() {
  // Check if we should reset the isPlaying flag
  if (isPlaying && (millis() - soundStartTime > SOUND_DURATION)) {
    isPlaying = false;
    Serial.println("Sound playback timeout, ready for next sound");
  }
}

// Function to get the total number of sound files
uint16_t getTotalSoundCount() {
  // Return cached count, or update if it's zero
  if (totalSoundCount == 0) {
    totalSoundCount = dfPlayer.readFileCounts();
  }
  
  // Ensure we return at least 1 to avoid division by zero
  return (totalSoundCount > 0) ? totalSoundCount : 1;
}
