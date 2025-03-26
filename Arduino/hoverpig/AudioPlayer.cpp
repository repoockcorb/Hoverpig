#include "AudioPlayer.h"

// Create the HardwareSerial and DFPlayer objects
// Use Serial2 for the DFPlayer Mini
HardwareSerial audioSerial(2); // Using Serial2
DFRobotDFPlayerMini audioPlayer;

// File management
const int numFiles = 5; // Total number of MP3 files on the SD card
int currentFileIndex = 1; // DFPlayer uses 1-based indexing for files
bool isPlaying = false;  // Track if audio is currently playing

void setupAudio() {
  Serial.println("Initializing DFPlayer Mini...");
  
  // Initialize serial communication with DFPlayer Mini
  // DFPlayer Mini requires 9600 baud rate
  audioSerial.begin(9600, SERIAL_8N1, PIN_MP3_RX, PIN_MP3_TX);
  delay(1000); // Give it time to initialize
  
  // Start communication with DFPlayer Mini
  if (audioPlayer.begin(audioSerial)) {
    Serial.println("DFPlayer Mini initialized successfully");
    
    // Set volume (0-30)
    audioPlayer.volume(30);
    Serial.println("Volume set to 30");
    
    // Check SD card and files
    checkSDCard();
    
    // Initialize random seed using analog noise from an unconnected pin
    randomSeed(analogRead(A0));
    
    // Don't automatically play - wait for button press
    Serial.println("Audio ready - press A button to play a random file");
  } else {
    Serial.println("Failed to initialize DFPlayer Mini!");
    Serial.println("Please check connections and SD card");
    
    // Try to diagnose the issue
    Serial.println("Troubleshooting tips:");
    Serial.println("1. Make sure SD card is formatted as FAT16/FAT32");
    Serial.println("2. Files should be named 001.mp3, 002.mp3, etc.");
    Serial.println("3. Check RX/TX connections are correct");
    Serial.println("4. Verify SD card is properly inserted");
    
    // Try to reset the DFPlayer
    Serial.println("Attempting to reset DFPlayer...");
    audioPlayer.reset();
    delay(2000);
    if (audioPlayer.begin(audioSerial)) {
      Serial.println("DFPlayer reset successful!");
    } else {
      Serial.println("DFPlayer reset failed.");
    }
  }
}

void checkSDCard() {
  // Check SD card status
  if (audioPlayer.available()) {
    Serial.println("SD card is available");
  } else {
    Serial.println("SD card might not be detected");
  }
  
  // List the number of files
  uint16_t fileCount = audioPlayer.readFileCounts();
  Serial.print("Number of files on SD card: ");
  Serial.println(fileCount);
  
  if (fileCount == 0) {
    Serial.println("No files found! Please check that:");
    Serial.println("1. SD card is formatted as FAT16/FAT32");
    Serial.println("2. Files are in the root directory");
    Serial.println("3. Files are named 001.mp3, 002.mp3, etc.");
    Serial.println("4. Files are valid MP3 format");
  } else {
    Serial.println("Files detected on SD card");
  }
}

void loopAudio() {
  // This function is called in the main loop to handle DFPlayer events
  
  // Check for DFPlayer Mini errors and events
  if (audioPlayer.available()) {
    uint8_t type = audioPlayer.readType();
    int value = audioPlayer.read();
    
    switch (type) {
      case DFPlayerError:
        Serial.print("DFPlayer Error: ");
        switch (value) {
          case Busy:
            Serial.println("Card not found");
            break;
          case Sleeping:
            Serial.println("Sleeping");
            break;
          case SerialWrongStack:
            Serial.println("Get Wrong Stack");
            break;
          case CheckSumNotMatch:
            Serial.println("Check Sum Not Match");
            break;
          case FileIndexOut:
            Serial.println("File Index Out of Bound");
            break;
          case FileMismatch:
            Serial.println("Cannot Find File");
            break;
          case Advertise:
            Serial.println("In Advertise");
            break;
          default:
            Serial.println("Unknown error");
            break;
        }
        break;
      case DFPlayerPlayFinished:
        Serial.print("Play finished for file: ");
        Serial.println(value);
        // Mark as not playing when finished
        isPlaying = false;
        Serial.println("Ready for next button press to play a random file");
        break;
    }
  }
}

void playRandomFile() {
  // Get the total number of files
  uint16_t fileCount = audioPlayer.readFileCounts();
  
  if (fileCount > 0) {
    // Generate a random file index between 1 and fileCount (inclusive)
    int randomIndex = random(1, fileCount + 1);
    
    Serial.print("Playing random file: ");
    Serial.println(randomIndex);
    
    // Play the random file
    audioPlayer.play(randomIndex);
    isPlaying = true;
    
    // Additional debug info
    if (audioPlayer.available()) {
      Serial.println("Player is available after play command");
    } else {
      Serial.println("Player might be busy or not responding");
      
      // Try alternative play method
      Serial.println("Trying alternative play method...");
      audioPlayer.playFolder(1, randomIndex);
      delay(100);
      
      // Check if that worked
      if (audioPlayer.available()) {
        Serial.println("Alternative play method seems to have worked");
      }
    }
  } else {
    Serial.println("No files found on SD card!");
  }
}

void playCurrentFile() {
  Serial.print("Playing file: ");
  Serial.println(currentFileIndex);
  
  // Try to play the file
  audioPlayer.play(currentFileIndex);
  isPlaying = true;
  
  // Additional debug info
  if (audioPlayer.available()) {
    Serial.println("Player is available after play command");
  } else {
    Serial.println("Player might be busy or not responding");
    
    // Try alternative play method
    Serial.println("Trying alternative play method...");
    audioPlayer.playFolder(1, currentFileIndex);
    delay(100);
    
    // Check if that worked
    if (audioPlayer.available()) {
      Serial.println("Alternative play method seems to have worked");
    }
  }
}

void stopPlayback() {
  if (isPlaying) {
    Serial.println("Stopping audio playback");
    audioPlayer.pause();
    isPlaying = false;
  }
}

void resumePlayback() {
  if (!isPlaying) {
    Serial.println("Resuming audio playback");
    audioPlayer.start();
    isPlaying = true;
  }
}

void nextFile() {
  // Move to the next file (with wrap-around)
  currentFileIndex = (currentFileIndex % numFiles) + 1;
  playCurrentFile();
}

void setVolume(uint8_t volume) {
  // Ensure volume is within valid range (0-30)
  if (volume > 30) volume = 30;
  audioPlayer.volume(volume);
  Serial.print("Volume set to: ");
  Serial.println(volume);
}
