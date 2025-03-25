/*
 * VEML6040 Color Sensor Calibration Functions
 * 
 * This file contains functions for calibrating the VEML6040 color sensor.
 * It's designed to be used by calling startCalibration() and then calling
 * updateCalibration() regularly from the main loop.
 */

#include "ColorCalibration.h" // Include own header file
#include <Arduino.h>
#include <Wire.h>
#include "veml6040.h"
#include "esp_task_wdt.h"  // Include watchdog timer
#include "SPIFFS.h"        // Include SPIFFS file system
#include "ColorSensor.h"   // For accessing shared variables

// For calibration
float calibrationRSum = 0, calibrationGSum = 0, calibrationBSum = 0;
int calibrationSamples = 0;
const int CALIBRATION_TOTAL_SAMPLES = 100; // Reduced from 50 to make calibration faster
// Use extern for calibrationActive since it's defined in ColorSensor.cpp
extern bool calibrationActive;
bool calibrationComplete = false;
unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL = 200; // Increased from 100ms to 200ms to reduce BT traffic
const unsigned long BT_COOLDOWN = 50;      // Cooldown time for Bluetooth buffer to clear

// Reference the calibration factors defined in ColorSensor.cpp
// No need to define them here, just reference the external variables
extern float rScale;
extern float gScale;
extern float bScale;
extern bool calibrationFactorsLoaded;

// Calibration file path
const char* CALIBRATION_FILE = "/color_calibration.txt";

VEML6040 calibrationSensor;

// Utility function to allow the Bluetooth stack to process events
void allowBTProcessing() {
  esp_task_wdt_reset();  // Reset watchdog
  delay(BT_COOLDOWN);    // Give time for BT stack to process events
  esp_task_wdt_reset();  // Reset watchdog again after delay
}

// Function to save calibration factors to SPIFFS
bool saveCalibrationFactors(float r, float g, float b) {
  // Ensure SPIFFS is started
  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS");
    return false;
  }
  
  // Open file for writing
  File file = SPIFFS.open(CALIBRATION_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open calibration file for writing");
    return false;
  }
  
  // Write calibration factors
  file.print(r, 6);
  file.print(",");
  file.print(g, 6);
  file.print(",");
  file.print(b, 6);
  file.close();
  
  Serial.println("Calibration factors saved to SPIFFS");
  return true;
}

// Function to load calibration factors from SPIFFS
bool loadCalibrationFactors(float &r, float &g, float &b) {
  // Ensure SPIFFS is started
  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS");
    return false;
  }
  
  // Check if file exists
  if (!SPIFFS.exists(CALIBRATION_FILE)) {
    Serial.println("Calibration file not found");
    return false;
  }
  
  // Open file for reading
  File file = SPIFFS.open(CALIBRATION_FILE, FILE_READ);
  if (!file) {
    Serial.println("Failed to open calibration file for reading");
    return false;
  }
  
  // Read calibration factors
  String content = file.readStringUntil('\n');
  file.close();
  
  // Parse calibration factors
  int firstComma = content.indexOf(',');
  int secondComma = content.indexOf(',', firstComma + 1);
  
  if (firstComma == -1 || secondComma == -1) {
    Serial.println("Invalid calibration file format");
    return false;
  }
  
  r = content.substring(0, firstComma).toFloat();
  g = content.substring(firstComma + 1, secondComma).toFloat();
  b = content.substring(secondComma + 1).toFloat();
  
  // Validate the loaded values
  if (r <= 0.0 || g <= 0.0 || b <= 0.0 || r > 10.0 || g > 10.0 || b > 10.0) {
    Serial.println("Calibration values out of expected range");
    return false;
  }
  
  Serial.println("Calibration factors loaded from SPIFFS:");
  Serial.print("rScale = ");
  Serial.println(r, 4);
  Serial.print("gScale = ");
  Serial.println(g, 4);
  Serial.print("bScale = ");
  Serial.println(b, 4);
  
  return true;
}

// Load calibration factors if available, to be called during setup
void initializeCalibration() {
  if (loadCalibrationFactors(rScale, gScale, bScale)) {
    calibrationFactorsLoaded = true;
    Serial.println("Using saved calibration factors from SPIFFS");
  } else {
    // Default values if no calibration file exists
    rScale = 1.0;
    gScale = 1.0;
    bScale = 1.0;
    calibrationFactorsLoaded = false;
    Serial.println("Using default calibration factors (1.0)");
  }
}

// Function to get the current calibration factors
void getCalibrationFactors(float &r, float &g, float &b) {
  r = rScale;
  g = gScale;
  b = bScale;
}

// Function to initialize the color sensor for calibration
void startCalibration() {
  Serial.println("Starting calibration process...");
  
  Wire.begin();
  
  if (!calibrationSensor.begin()) {
    Serial.println("ERROR: couldn't detect the sensor");
    return;
  }
  
  // Configure sensor: 40ms integration time, auto mode, sensor enabled
  calibrationSensor.setConfiguration(VEML6040_IT_40MS + VEML6040_AF_AUTO + VEML6040_SD_ENABLE);
  
  // Allow BT stack to process after initial setup
  allowBTProcessing();
  
  Serial.println("Vishay VEML6040 sensor initialized for calibration.");
  
  // Allow BT stack to process after first message
  allowBTProcessing();
  
  Serial.println("Place a WHITE PAPER in front of the sensor as reference surface.");
  Serial.println("Hold it about 1-2cm from the sensor and keep it steady.");
  
  // Allow BT stack to process after second message
  allowBTProcessing();
  
  Serial.print("Taking ");
  Serial.print(CALIBRATION_TOTAL_SAMPLES);
  Serial.println(" samples for calibration...");
  
  // Reset values
  calibrationRSum = 0;
  calibrationGSum = 0;
  calibrationBSum = 0;
  calibrationSamples = 0;
  calibrationComplete = false;
  calibrationActive = true;
  lastSampleTime = millis();
  
  // Give BT stack time to process before starting sampling
  allowBTProcessing();
}

// Function to be called regularly to update the calibration process
// Returns true if calibration is complete
bool updateCalibration() {
  // Reset the watchdog timer
  esp_task_wdt_reset();
  
  // If not in calibration mode, do nothing
  if (!calibrationActive) {
    return calibrationComplete;
  }
  
  // If already completed, return true
  if (calibrationComplete) {
    calibrationActive = false;
    return true;
  }
  
  // Check if it's time to take another sample
  unsigned long currentTime = millis();
  if (currentTime - lastSampleTime < SAMPLE_INTERVAL && calibrationSamples > 0) {
    return false; // Not time for a new sample yet
  }
  
  // Take a sample
  if (calibrationSamples < CALIBRATION_TOTAL_SAMPLES) {
    // Reset the watchdog timer
    esp_task_wdt_reset();
    
    // Read raw sensor values
    uint16_t rawRed   = calibrationSensor.getRed();
    uint16_t rawGreen = calibrationSensor.getGreen();
    uint16_t rawBlue  = calibrationSensor.getBlue();
    uint16_t rawWhite = calibrationSensor.getWhite();
    
    // Accumulate values
    calibrationRSum += rawRed;
    calibrationGSum += rawGreen;
    calibrationBSum += rawBlue;
    
    // Only print every 3rd sample to reduce BT traffic
    if (calibrationSamples % 3 == 0) {
      Serial.print("Sample ");
      Serial.print(calibrationSamples + 1);
      Serial.print("/");
      Serial.print(CALIBRATION_TOTAL_SAMPLES);
      Serial.print(" - R: ");
      Serial.print(rawRed);
      Serial.print(" G: ");
      Serial.print(rawGreen);
      Serial.print(" B: ");
      Serial.println(rawBlue);
      
      // Allow BT stack to process after printing
      allowBTProcessing();
    }
    
    calibrationSamples++;
    lastSampleTime = currentTime;
    
    return false; // Not complete yet
  } 
  else if (!calibrationComplete) {
    // Reset the watchdog timer
    esp_task_wdt_reset();
    
    // Calculate averages
    float avgRed = calibrationRSum / CALIBRATION_TOTAL_SAMPLES;
    float avgGreen = calibrationGSum / CALIBRATION_TOTAL_SAMPLES;
    float avgBlue = calibrationBSum / CALIBRATION_TOTAL_SAMPLES;
    
    // Calculate the overall average
    float overallAvg = (avgRed + avgGreen + avgBlue) / 3;
    
    // Calculate scaling factors
    rScale = overallAvg / avgRed;
    gScale = overallAvg / avgGreen;
    bScale = overallAvg / avgBlue;
    
    Serial.println("\n----- CALIBRATION RESULTS -----");
    allowBTProcessing();
    
    Serial.print("Average Red: ");
    Serial.println(avgRed);
    allowBTProcessing();
    
    Serial.print("Average Green: ");
    Serial.println(avgGreen);
    allowBTProcessing();
    
    Serial.print("Average Blue: ");
    Serial.println(avgBlue);
    allowBTProcessing();
    
    Serial.print("Overall Average: ");
    Serial.println(overallAvg);
    allowBTProcessing();
    
    Serial.println("\n----- SCALING FACTORS -----");
    allowBTProcessing();
    
    Serial.print("rScale = ");
    Serial.println(rScale, 4);
    allowBTProcessing();
    
    Serial.print("gScale = ");
    Serial.println(gScale, 4);
    allowBTProcessing();
    
    Serial.print("bScale = ");
    Serial.println(bScale, 4);
    allowBTProcessing();
    
    // Save calibration factors to SPIFFS
    if (saveCalibrationFactors(rScale, gScale, bScale)) {
      Serial.println("Calibration factors saved to SPIFFS");
    } else {
      Serial.println("Failed to save calibration factors");
    }
    allowBTProcessing();
    
    calibrationFactorsLoaded = true;
    calibrationComplete = true;
    calibrationActive = false;
    Serial.println("Calibration process complete!");
    allowBTProcessing();
    
    return true; // Calibration process complete
  }
  
  return calibrationComplete;
}

// For compatibility with old code
void runCalibration() {
  Serial.println("WARNING: Using blocking calibration method, which may cause the ESP32 to reset");
  Serial.println("Consider using startCalibration() and updateCalibration() instead");
  allowBTProcessing();
  
  startCalibration();
  
  // This is a blocking approach, which can trigger the watchdog timer
  while (!updateCalibration()) {
    delay(10);
    // Reset watchdog timer while in this loop
    esp_task_wdt_reset();
  }
}