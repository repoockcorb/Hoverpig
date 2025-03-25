#ifndef COLOR_CALIBRATION_H
#define COLOR_CALIBRATION_H

#include <Arduino.h>
#include <Wire.h>
#include "veml6040.h"
#include "SPIFFS.h"
#include "ColorSensor.h"

// Function declarations
void allowBTProcessing();
bool saveCalibrationFactors(float r, float g, float b);
bool loadCalibrationFactors(float &r, float &g, float &b);
void initializeCalibration();
void getCalibrationFactors(float &r, float &g, float &b);
void startCalibration();
bool updateCalibration();
void runCalibration();

// Constants
extern const char* CALIBRATION_FILE;
extern const int CALIBRATION_TOTAL_SAMPLES;

#endif // COLOR_CALIBRATION_H 