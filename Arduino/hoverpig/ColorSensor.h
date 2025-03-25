#ifndef COLOR_SENSOR_H
#define COLOR_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "veml6040.h"

// Configurable settings for sensor timing
extern unsigned long COLOR_SENSOR_READ_INTERVAL;  // Time between sensor reads in ms
extern unsigned long COLOR_SENSOR_TASK_INTERVAL;  // Time between task iterations in ms

// Configurable color detection thresholds
extern float RED_DETECTION_RATIO_THRESHOLD;  // Higher = stricter red detection
extern float COLOR_INTENSITY_THRESHOLD;      // Minimum intensity for color detection

// Function prototypes
void setupColorSensor();
void startColorSensorTask();
void updateColorSensor();
bool isBoostColor();
String getDetectedColor();
bool hasColorChanged();
void resetColorChanged();
void getColorRGB(byte &r, byte &g, byte &b);

// Calibration functions
void initializeCalibration();
void runCalibration();
void saveCalibrationFactors();
void loadCalibrationFactors();
void startCalibration();
bool updateCalibration();

// External variables for boost state
extern bool boostActive;
extern unsigned long boostStartTime;
extern const unsigned long BOOST_DURATION;

// Thread control
void colorSensorTask(void *pvParameters); // Task function that runs on Core 0

// Calibration storage functions
void getCalibrationFactors(float &r, float &g, float &b); // Get current factors

// External declarations
extern VEML6040 colorSensor;
extern String currentColorName;   // The current detected color name
extern bool calibrationActive;    // Flag indicating if calibration is in progress
extern float rScale, gScale, bScale; // Calibration scaling factors
extern bool calibrationFactorsLoaded; // Indicates if calibration was loaded
extern bool colorChanged;         // Flag indicating if color has changed
extern SemaphoreHandle_t colorMutex; // Mutex to protect shared color data

#endif // COLOR_SENSOR_H