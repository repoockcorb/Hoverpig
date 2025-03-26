#include "ColorSensor.h"
#include "AudioPlayer.h"  // Include for playing sounds
#include "GamepadController.h"  // Include for rumble control

// Threading support
TaskHandle_t colorTaskHandle = NULL;
SemaphoreHandle_t colorMutex = NULL;

// External reference to controller connection state
extern volatile bool controllerConnecting;

// Initialize the sensor
VEML6040 colorSensor;

// Global variables
String currentColorName = "None";
String previousColorName = "None";  // Track previous color to detect changes
bool boostActive = false;
unsigned long boostStartTime = 0;
const unsigned long BOOST_DURATION = 1500; // Boost lasts for 3 seconds

// Rumble control
unsigned long rumbleStartTime = 0;
const unsigned long RUMBLE_DURATION = 400; // Rumble duration in milliseconds
bool rumbleActive = false;

// Track color change for LED updates
bool colorChanged = false;

// Error recovery
unsigned long lastSensorErrorTime = 0;
bool sensorErrorDetected = false;
const unsigned long SENSOR_RECOVERY_INTERVAL = 2000; // Try to recover sensor every 2 seconds

// Calibration factors - these are defined here and referenced in ColorCalibration.cpp
float rScale = 1.0;  // Calibration factor for red
float gScale = 1.0;  // Calibration factor for green
float bScale = 1.0;  // Calibration factor for blue
bool calibrationFactorsLoaded = false;
bool calibrationActive = false; // Flag indicating if calibration is in progress

// Store the latest color values
float latestRed = 0;
float latestGreen = 0;
float latestBlue = 0;

// Add configurable timing constants at the top of the file
unsigned long COLOR_SENSOR_READ_INTERVAL = 25;  // Default to 25ms between reads (40Hz)
unsigned long COLOR_SENSOR_TASK_INTERVAL = 20;  // Default to 20ms between task iterations

// Configurable thresholds for color detection - SIGNIFICANTLY LOWERED for better detection
float RED_DETECTION_RATIO_THRESHOLD = 1.5;      // Lower threshold for red
float GREEN_DETECTION_RATIO_THRESHOLD = 1.1;    // Significantly lower threshold for green
float BLUE_DETECTION_RATIO_THRESHOLD = 1.1;     // Significantly lower threshold for blue
float COLOR_INTENSITY_THRESHOLD = 10.0;         // Very low minimum threshold for better detection

// Debug flags
bool COLOR_DEBUG_VERBOSE = false;               // Enable detailed color information to debug white detection
int COLOR_DEBUG_FREQUENCY = 10;                 // Print every 10th reading (increased frequency)

// Add global scaling factors to enhance blue/green detection
float BLUE_BOOST_FACTOR = 1.0;                 // Removed blue boost to prevent blue bias in white detection
float GREEN_BOOST_FACTOR = 1.0;                // Slightly reduced green boost as well

// Helper function that estimates a color name from calibrated RGB values
String getColorName(float red, float green, float blue) {
  // Apply additional boosting to blue and green channels to compensate for sensor bias
  blue *= BLUE_BOOST_FACTOR;   // Boost blue channel
  green *= GREEN_BOOST_FACTOR; // Boost green channel
  
  // Save the latest values for getColorRGB function (store the original values)
  latestRed = red;
  latestGreen = green / GREEN_BOOST_FACTOR;  // Store original value
  latestBlue = blue / BLUE_BOOST_FACTOR;     // Store original value
  
  // Calculate the total intensity with boosted values
  float sum = red + green + blue;
  
  // Debug output for ALL readings to diagnose issues
  static int debugCounter = 0;
  debugCounter++;
  
  // Basic intensity check with very low threshold
  if (sum < COLOR_INTENSITY_THRESHOLD) {
    if (COLOR_DEBUG_VERBOSE && debugCounter % COLOR_DEBUG_FREQUENCY == 0) {
      Serial.print("LOW INTENSITY: R:");
      Serial.print(red);
      Serial.print(" G:");
      Serial.print(green);
      Serial.print(" B:");
      Serial.print(blue);
      Serial.print(" Sum:");
      Serial.println(sum);
    }
    return "None";  // No color detected or too dark
  }
  
  // MANDATORY debugging for every color reading to diagnose the issue
  if (COLOR_DEBUG_VERBOSE && debugCounter % COLOR_DEBUG_FREQUENCY == 0) {
    Serial.print("Raw RGB: R:");
    Serial.print(red);
    Serial.print(" G:");
    Serial.print(green);
    Serial.print(" B:");
    Serial.print(blue);
    Serial.print(" Sum:");
    Serial.print(sum);
  }
  
  // SUPER SIMPLE COLOR DETECTION - Just check which channel is dominant
  // with slightly different thresholds for different colors
  
  // Blue check - made EXTRA lenient (only needs to be 1.03x stronger)
  if (blue > red * 1.03 && blue > green * 1.03 && blue > 10) {
    if (COLOR_DEBUG_VERBOSE && debugCounter % COLOR_DEBUG_FREQUENCY == 0) {
      Serial.println(" BLUE detected");
    }
    return "Blue";
  }
  
  // Green check - also very lenient
  if (green > red * 1.03 && green > blue * 1.03 && green > 10) {
    if (COLOR_DEBUG_VERBOSE && debugCounter % COLOR_DEBUG_FREQUENCY == 0) {
      Serial.println(" GREEN detected");
    }
    return "Green";
  }
  
  // Red check
  if (red > green * 1.3 && red > blue * 1.3 && red > 25) {
    if (COLOR_DEBUG_VERBOSE && debugCounter % COLOR_DEBUG_FREQUENCY == 0) {
      Serial.println(" RED detected");
    }
    return "Red";
  }
  
  // Check for near-equal values first (white/gray detection)
  float maxVal = max(max(red, green), blue);
  float minVal = min(min(red, green), blue);
  float maxDiff = maxVal - minVal;
  
  // More robust white detection
  if (maxDiff < (sum * 0.25) && sum > 60) {
    // Additional check to ensure values are reasonably balanced
    if (red > (sum * 0.25) && green > (sum * 0.25) && blue > (sum * 0.25)) {
      if (COLOR_DEBUG_VERBOSE && debugCounter % COLOR_DEBUG_FREQUENCY == 0) {
        Serial.println(" WHITE detected");
        Serial.print(" MaxDiff ratio: "); 
        Serial.println(maxDiff / sum);
      }
      return "White";
    }
  }
  
  // Mixed color detection with priority for blue and green
  
  // Cyan (green + blue)
  if (green > 15 && blue > 15 && green > red * 1.2 && blue > red * 1.2) {
    if (COLOR_DEBUG_VERBOSE && debugCounter % COLOR_DEBUG_FREQUENCY == 0) {
      Serial.println(" CYAN detected");
    }
    return "Cyan";
  }
  
  // Purple/Magenta (red + blue)
  if (red > 15 && blue > 15 && red > green * 1.2 && blue > green * 1.2) {
    if (COLOR_DEBUG_VERBOSE && debugCounter % COLOR_DEBUG_FREQUENCY == 0) {
      Serial.println(" PURPLE detected");
    }
    return "Purple";
  }
  
  // Yellow (red + green)
  if (red > 15 && green > 15 && red > blue * 1.2 && green > blue * 1.2) {
    if (COLOR_DEBUG_VERBOSE && debugCounter % COLOR_DEBUG_FREQUENCY == 0) {
      Serial.println(" YELLOW detected");
    }
    return "Yellow";
  }
  
  // Orange (red stronger than green)
  if (red > 20 && green > 10 && red > green * 1.2 && red > blue * 1.5) {
    if (COLOR_DEBUG_VERBOSE && debugCounter % COLOR_DEBUG_FREQUENCY == 0) {
      Serial.println(" ORANGE detected");
    }
    return "Orange";
  }
  
  // Dominance-based approach - which color is strongest?
  String dominantColor = "None";
  if (red > green && red > blue) {
    dominantColor = "Red";
  } else if (green > red && green > blue) {
    dominantColor = "Green";
  } else if (blue > red && blue > green) {
    dominantColor = "Blue";
  }
  
  // Default to the dominant color if we don't have a better match
  if (dominantColor != "None") {
    if (COLOR_DEBUG_VERBOSE && debugCounter % COLOR_DEBUG_FREQUENCY == 0) {
      Serial.print(" Default to DOMINANT: ");
      Serial.println(dominantColor);
    }
    return dominantColor;
  }
  
  // If no color detected with our simple rules, just return the closest match based on RGB
  if (COLOR_DEBUG_VERBOSE && debugCounter % COLOR_DEBUG_FREQUENCY == 0) {
    Serial.println(" No clear color detected");
  }
  return "None";
}

void setupColorSensor() {
  // Load calibration factors before initializing the sensor
  initializeCalibration();
  
  // Create mutex for thread-safe access to color data
  colorMutex = xSemaphoreCreateMutex();
  if (colorMutex == NULL) {
    Serial.println("ERROR: Failed to create color mutex");
  }
  
  Wire.begin();
  Wire.setClock(400000); // Increase I2C clock speed to 400kHz for faster communication
  
  if (!colorSensor.begin()) {
    Serial.println("ERROR: couldn't detect the color sensor");
    sensorErrorDetected = true;
    lastSensorErrorTime = millis();
    // Don't halt - other functions should continue to work
  } else {
    // Use shorter integration time (80ms) for better handling of bright objects
    // This reduces saturation when white paper is close to the sensor
    colorSensor.setConfiguration(VEML6040_IT_80MS + VEML6040_AF_AUTO + VEML6040_SD_ENABLE);
    
    // Use delay to allow sensor to stabilize with new settings
    delay(50); 
    
    Serial.println("Vishay VEML6040 color sensor initialized");
    Serial.println("Using SHORTER INTEGRATION TIME to prevent saturation");
    Serial.println("Applying boost factors - Blue: " + String(BLUE_BOOST_FACTOR) + "x, Green: " + String(GREEN_BOOST_FACTOR) + "x");
    Serial.println("Calibration factors:");
    Serial.print("  rScale: "); Serial.println(rScale);
    Serial.print("  gScale: "); Serial.println(gScale);
    Serial.print("  bScale: "); Serial.println(bScale);
    
    // Print initial raw readings to verify sensor is working
    delay(100); // Wait for first reading with new integration time
    uint16_t r = colorSensor.getRed();
    uint16_t g = colorSensor.getGreen();
    uint16_t b = colorSensor.getBlue();
    uint16_t w = colorSensor.getWhite();
    
    Serial.println("Initial sensor readings:");
    Serial.print("  R: "); Serial.println(r);
    Serial.print("  G: "); Serial.println(g);
    Serial.print("  B: "); Serial.println(b);
    Serial.print("  W: "); Serial.println(w);
    
    // Print boosted values
    Serial.println("After boosting:");
    Serial.print("  R: "); Serial.println(r);
    Serial.print("  G: "); Serial.println(g * GREEN_BOOST_FACTOR);
    Serial.print("  B: "); Serial.println(b * BLUE_BOOST_FACTOR);
  }
  
  // Start the color sensor task on Core 0
  startColorSensorTask();
}

// Thread function for the color sensor that runs on Core 0
void colorSensorTask(void *pvParameters) {
  Serial.println("Color sensor task started on Core 0");
  
  // Use regular polling with task-friendly delays instead of precise timing
  // This is more resilient to other task delays
  
  for (;;) {
    // Only run sensor updates when not in controller connection process
    if (!controllerConnecting) {
      // Single check per loop
      updateColorSensor();
      
      // Try to recover sensor if it failed previously
      if (sensorErrorDetected) {
        unsigned long currentTime = millis();
        if (currentTime - lastSensorErrorTime > SENSOR_RECOVERY_INTERVAL) {
          // Try to re-initialize the sensor
          if (colorSensor.begin()) {
            // Use shorter integration time to prevent saturation with bright objects
            colorSensor.setConfiguration(VEML6040_IT_80MS + VEML6040_AF_AUTO + VEML6040_SD_ENABLE);
            Serial.println("Color sensor recovered successfully");
            
            // Print readings after recovery to verify
            delay(100); // Wait for first reading
            uint16_t r = colorSensor.getRed();
            uint16_t g = colorSensor.getGreen();
            uint16_t b = colorSensor.getBlue();
            uint16_t w = colorSensor.getWhite();
            
            Serial.println("Sensor readings after recovery:");
            Serial.print("  R: "); Serial.println(r);
            Serial.print("  G: "); Serial.println(g);
            Serial.print("  B: "); Serial.println(b);
            Serial.print("  W: "); Serial.println(w);
            
            // Print boosted values that will be used for color detection
            Serial.println("After boosting:");
            Serial.print("  R: "); Serial.println(r);
            Serial.print("  G: "); Serial.println(g * GREEN_BOOST_FACTOR);
            Serial.print("  B: "); Serial.println(b * BLUE_BOOST_FACTOR);
            
            sensorErrorDetected = false;
          } else {
            Serial.println("Color sensor recovery failed, will retry later");
            lastSensorErrorTime = currentTime;
          }
        }
      }
    }
    
    // Use a configurable delay (not delayUntil) to be more resilient
    vTaskDelay(COLOR_SENSOR_TASK_INTERVAL / portTICK_PERIOD_MS);
  }
}

// Start the color sensor task on Core 0
void startColorSensorTask() {
  // Create a task for the color sensor that runs on Core 0
  xTaskCreatePinnedToCore(
    colorSensorTask,    // Function to implement the task
    "ColorSensorTask",  // Name of the task
    4096,               // Stack size in words
    NULL,               // Task input parameter
    1,                  // Priority of the task (lower than controller task)
    &colorTaskHandle,   // Task handle
    0);                 // Core where the task should run (0 = Core 0)
}

// This function is now run in its own thread but with safety checks
void updateColorSensor() {
  static unsigned long lastReadTime = 0;
  static bool wasSaturated = false; // Track previous saturation state
  unsigned long currentTime = millis();
  
  // Use configurable read interval
  if (currentTime - lastReadTime >= COLOR_SENSOR_READ_INTERVAL) {
    lastReadTime = currentTime;
    
    // Read raw sensor values with error handling
    uint16_t rawRed = 0, rawGreen = 0, rawBlue = 0, rawWhite = 0;
    bool readSuccess = true;
    bool sensorSaturated = false;
    
    // Read sensor with improved error handling
    rawRed = colorSensor.getRed();
    // Only treat as error if reading returns 0xFFFF but NOT if it's just a very high value
    // High values might indicate saturation (bright white object), which isn't an error
    if (rawRed == 0xFFFF) {
      // Could be error or saturation - check other channels
      rawGreen = colorSensor.getGreen();
      rawBlue = colorSensor.getBlue();
      rawWhite = colorSensor.getWhite();
      
      // If all channels are maxed out, this is likely saturation (too close white object)
      if (rawGreen >= 65000 && rawBlue >= 65000 && rawWhite >= 65000) {
        sensorSaturated = true;
        
        // Only print saturation message when state changes from not saturated to saturated
        if (!wasSaturated) {
          Serial.println("Sensor saturated - object too close or too bright");
        }
        
        // Treat as white and continue
        rawRed = 65000;
        rawGreen = 65000;
        rawBlue = 65000;
      } else {
        // True error - something is wrong with the sensor
        readSuccess = false;
      }
    } else {
      // Normal reading - continue with other channels
      rawGreen = colorSensor.getGreen();
      if (rawGreen == 0xFFFF && !sensorSaturated) {
        readSuccess = false;
      }
      
      if (readSuccess) {
        rawBlue = colorSensor.getBlue();
        if (rawBlue == 0xFFFF && !sensorSaturated) {
          readSuccess = false;
        }
      }
      
      if (readSuccess) {
        rawWhite = colorSensor.getWhite();
        if (rawWhite == 0xFFFF && !sensorSaturated) {
          readSuccess = false;
        }
      }
    }
    
    // Store current saturation state for next time
    wasSaturated = sensorSaturated;
    
    // Check if all readings were successful or we have a saturation case
    if (!readSuccess && !sensorSaturated) {
      Serial.println("ERROR: Failed to read color sensor");
      sensorErrorDetected = true;
      lastSensorErrorTime = currentTime;
      return;
    }
    
    // Only print RAW values when debug is explicitly enabled
    static int rawDebugCounter = 0;
    if (COLOR_DEBUG_VERBOSE && rawDebugCounter++ % 50 == 0) {
      Serial.println("=== RAW SENSOR VALUES ===");
      Serial.print("R: "); Serial.print(rawRed);
      Serial.print(" G: "); Serial.print(rawGreen);
      Serial.print(" B: "); Serial.print(rawBlue);
      Serial.print(" W: "); Serial.println(rawWhite);
      Serial.print("Calibration - rScale: "); Serial.print(rScale);
      Serial.print(" gScale: "); Serial.print(gScale);
      Serial.print(" bScale: "); Serial.println(bScale);
      if (sensorSaturated) {
        Serial.println("SENSOR SATURATED - object likely too close");
      }
    }
    
    // Apply calibration factors to the raw values
    float calibratedRed, calibratedGreen, calibratedBlue;
    
    if (sensorSaturated) {
      // For saturated readings, use high fixed values that will definitely yield "white"
      calibratedRed = 1000;
      calibratedGreen = 1000;
      calibratedBlue = 1000;
    } else {
      calibratedRed = rawRed * rScale;
      calibratedGreen = rawGreen * gScale;
      calibratedBlue = rawBlue * bScale;
    }

    // Get color name from calibrated values
    String newColorName = sensorSaturated ? "White" : getColorName(calibratedRed, calibratedGreen, calibratedBlue);
    
    // Take the mutex before modifying shared data
    if (xSemaphoreTake(colorMutex, 20 / portTICK_PERIOD_MS) == pdTRUE) { // Increased timeout
      // Check if color has changed before updating
      if (newColorName != currentColorName) {
        previousColorName = currentColorName;
        currentColorName = newColorName;
        
        // Set the color changed flag for immediate LED update
        colorChanged = true;
        
        // ALWAYS print when color changes - this is the only output the user wants
        // Serial.print("Color changed: ");
        // Serial.print(previousColorName);
        // Serial.print(" -> ");
        // Serial.println(currentColorName);
        
        // Check for red detection - immediate reaction
        if (currentColorName == "Red" && previousColorName != "Red" && !controllerConnecting) {
          // Only do one operation at a time to reduce BT traffic
          static unsigned long lastBtOperation = 0;
          
          // If no bluetooth operation in the last 100ms, perform next operation
          if (currentTime - lastBtOperation > 150) {
            // First handle rumble if not active
            if (!rumbleActive && myGamepad && myGamepad->isConnected()) {
              myGamepad->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */, 0x80 /* weakMagnitude */,
                          0x80 /* strongMagnitude */);
              rumbleActive = true;
              rumbleStartTime = currentTime;
              lastBtOperation = currentTime;
            } 
            // Then handle audio playback if rumble is already started
            else if (!isPlaying) {
              // playFile(3); // Play the specific file #3
              lastBtOperation = currentTime;
            }
          }
        }
        
        // Check if it's a boost color - immediate activation
        if (isBoostColor() && !boostActive) {
          boostActive = true;
          boostStartTime = millis();
        }
      }
      
      // Check if rumble time is over
      if (rumbleActive && (currentTime - rumbleStartTime >= RUMBLE_DURATION)) {
        if (myGamepad && myGamepad->isConnected()) {
          // Only send the command to stop rumble once
          static bool rumbleStopSent = false;
          if (!rumbleStopSent) {
            myGamepad->setRumble(0, 0); // Stop rumble
            rumbleStopSent = true;
          }
          rumbleActive = false;
        }
      } else if (rumbleActive) {
        // Reset the flag when rumble becomes active again
        static bool rumbleStopSent = false;
        rumbleStopSent = false;
      }
      
      // Check if boost time has expired
      if (boostActive && (millis() - boostStartTime >= BOOST_DURATION)) {
        boostActive = false;
      }
      
      // Release the mutex
      xSemaphoreGive(colorMutex);
    }
  }
}

bool isBoostColor() {
  bool result = false;
  
  // Thread-safe access to color data with timeout - don't hang if mutex unavailable
  if (xSemaphoreTake(colorMutex, 15 / portTICK_PERIOD_MS) == pdTRUE) {
    // Check if current color is red or orange - no debounce for faster response
    result = (currentColorName == "Red" || currentColorName == "Orange");
    xSemaphoreGive(colorMutex);
  }
  
  return result;
}

String getDetectedColor() {
  String result = "None";
  
  // Thread-safe access to color data with timeout - don't hang if mutex unavailable
  if (xSemaphoreTake(colorMutex, 15 / portTICK_PERIOD_MS) == pdTRUE) {
    // Return the current color name
    result = currentColorName;
    xSemaphoreGive(colorMutex);
  }
  
  return result;
}

// Check if color has changed since last read
bool hasColorChanged() {
  bool result = false;
  
  // Thread-safe access to color data with increased timeout
  if (xSemaphoreTake(colorMutex, 15 / portTICK_PERIOD_MS) == pdTRUE) {
    result = colorChanged;
    xSemaphoreGive(colorMutex);
  }
  
  return result;
}

// Reset the color changed flag after reading it
void resetColorChanged() {
  // Thread-safe access to color data
  if (xSemaphoreTake(colorMutex, 10 / portTICK_PERIOD_MS) == pdTRUE) { // 10ms timeout
    colorChanged = false;
    xSemaphoreGive(colorMutex);
  }
}

void getColorRGB(byte &r, byte &g, byte &b) {
  // Default values in case mutex can't be taken
  r = 0; g = 255; b = 0;  // Default to green
  
  // Thread-safe access to color data with timeout - don't hang if mutex unavailable
  if (xSemaphoreTake(colorMutex, 15 / portTICK_PERIOD_MS) == pdTRUE) {
    // Just use fixed colors for reliability
    if (currentColorName == "White") {
      r = 255; g = 255; b = 255;  // Prioritize White by checking it first
    } else if (currentColorName == "Red") {
      r = 255; g = 0; b = 0;
    } else if (currentColorName == "Green") {
      r = 0; g = 255; b = 0;
    } else if (currentColorName == "Blue") {
      r = 0; g = 0; b = 255;
    } else if (currentColorName == "Orange") {
      r = 255; g = 128; b = 0;
    } else if (currentColorName == "Yellow") {
      r = 255; g = 255; b = 0;
    } else if (currentColorName == "Purple") {
      r = 128; g = 0; b = 255;
    } else if (currentColorName == "Cyan") {
      r = 0; g = 255; b = 255;
    } else if (currentColorName == "Magenta") {
      r = 255; g = 0; b = 255;
    } else {
      // Default to green for all other colors
      r = 0; g = 255; b = 0;
    }
    
    // Debug information - print the assigned RGB values
    static String lastDebuggedColor = "";
    if (lastDebuggedColor != currentColorName) {
      Serial.print("LED COLOR SET for '");
      Serial.print(currentColorName);
      Serial.print("': R=");
      Serial.print(r);
      Serial.print(", G=");
      Serial.print(g);
      Serial.print(", B=");
      Serial.println(b);
      lastDebuggedColor = currentColorName;
    }
    
    xSemaphoreGive(colorMutex);
  }
}