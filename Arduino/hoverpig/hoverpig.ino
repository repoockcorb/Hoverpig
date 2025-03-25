// main.ino
#include "GamepadController.h"
#include "AudioPlayer.h"
#include "MotorController.h"
#include <ESP32Servo.h>
#include "DFRobotDFPlayerMini.h"
#include "ColorSensor.h"
#include "ColorCalibration.h"

#include "esp_system.h"
#include "esp_task_wdt.h"  // Include the watchdog timer library
#include "SPIFFS.h"        // Include SPIFFS file system

// Main task handle for controller communication
TaskHandle_t controllerTaskHandle = NULL;

// Connection flag to prevent task conflicts
volatile bool controllerConnecting = false;
volatile bool connectionPending = false;
GamepadPtr pendingGamepad = nullptr;

// Debug counter for tracking activity
unsigned long loopCounter = 0;
unsigned long lastDebugTime = 0;

const int BUTTON_DEBOUNCE_DELAY = 500;     // Debounce delay in milliseconds
unsigned long buttonlastDebounceTime = 0;  // Keeps track of the last time the button was pressed

const int DEADZONE_THRESHOLD = 40;  // Deadzone threshold for the controller sticks

int16_t motor_drive = 0;
int16_t motor_steer = 0;

int motor_speed = 0;
int update_remote_led = 1;

// Boost settings
int BOOST_SPEED_ADD = 150;  // Value to add during boost - adjustable
const int MAX_BOOST_SPEED = 450;  // Maximum allowed speed during boost
unsigned long BOOST_DURATION_AFTER_RED = 1000;  // How long boost continues after leaving red (ms)
unsigned long lastBoostColorTime = 0;  // Track when we last saw a boost color

// LED update timing
const unsigned long LED_UPDATE_INTERVAL = 80; // Reduced frequency for more responsive updates
unsigned long lastLedUpdateTime = 0;

// Store previous speed mode colors
byte speedModeR = 0;
byte speedModeG = 255;
byte speedModeB = 0;
bool speedModeChanged = false;

// Calibration state
bool calibrationInitiated = false;
bool calibrationFinished = false;
unsigned long calibrationEndTime = 0;

// Controller management
unsigned long lastControllerUpdateTime = 0;
const unsigned long CONTROLLER_UPDATE_INTERVAL = 100; // Update controller at most every 100ms during calibration

// Signal for safe mode activation
bool enterSafeMode = false;

// Custom modified GamepadController functions to avoid deadlocks
// This runs in the BTstack thread - we need to keep it minimal
void onConnectedGamepadModified(GamepadPtr gp) {
  // Just store reference and return immediately
  Serial.println("BT thread: Controller connected");
  myGamepad = gp;  
  connectionPending = true;
  controllerConnecting = true;
}

// Graceful connection handling in the main thread
void completeConnectionSetup() {
  if (!connectionPending) return;
  
  // We're handling this in the main thread now
  Serial.println("Main thread: Completing connection setup");
  
  // Wait before any BT operations
  delay(100);
  esp_task_wdt_reset();
  
  // Very limited operation - just set LED once
  if (myGamepad && myGamepad->isConnected()) {
    Serial.println("Setting controller LED to green");
    myGamepad->setColorLED(0, 255, 0);  // Green color
    delay(100);
    esp_task_wdt_reset();
  }
  
  Serial.println("Connection complete, resuming normal operation");
  connectionPending = false;
  controllerConnecting = false;
}

// Thread function for controller communication
void controllerTask(void *pvParameters) {
  // This task runs on Core 1 and handles controller and motor communication
  unsigned long taskStartTime = millis();
  unsigned long lastControllerPoll = 0;
  
  for (;;) {
    // Reset the watchdog
    esp_task_wdt_reset();
    unsigned long currentTime = millis();
    
    // Print debug info to track task activity
    // if (currentTime - lastDebugTime > 5000) {
    //   Serial.printf("Controller task running for %lu ms, loops: %lu\n", 
    //                currentTime - taskStartTime, loopCounter);
    //   lastDebugTime = currentTime;
    // }
    
    loopCounter++;
    
    // Skip processing if we're in connection process
    if (controllerConnecting) {
      vTaskDelay(20); // Just wait with longer delay
      esp_task_wdt_reset();
      continue;
    }
    
    // Controller polling - careful not to call too frequently
    if (currentTime - lastControllerPoll >= 20) { // Poll at 50Hz maximum
      BP32.update(); // This is the critical call that can block
      lastControllerPoll = currentTime;
    }
    
    // Check if we entered safe mode - skip most operations
    if (enterSafeMode) {
      // In safe mode, just update motors with neutral values
      Send(0, 0);
      vTaskDelay(50);
      continue;
    }
    
    // If we have a gamepad, process inputs
    if (myGamepad && myGamepad->isConnected()) {
      // Apply deadzone logic to the axis values
      int16_t axisYRaw = myGamepad->axisY() + leftJoystickYOffset;
      int16_t axisRXRaw = myGamepad->axisRX() + rightJoystickXOffset;
      
      int deadzonedAxisY = abs(axisYRaw) < DEADZONE_THRESHOLD ? 0 : axisYRaw;
      int deadzonedAxisRX = abs(axisRXRaw) < DEADZONE_THRESHOLD ? 0 : axisRXRaw;
      
      motor_drive = map(deadzonedAxisY, -500, 500, -285, 285);
      motor_steer = map(deadzonedAxisRX, -500, 500, -130, 130);
      
      // Apply boost when red is detected or within duration window after red
      bool boostColorDetected = isBoostColor();
      
      // Update last time we saw a boost color
      if (boostColorDetected) {
        lastBoostColorTime = currentTime;
      }
      
      // Check if we're in boost state (either on red or within duration window)
      bool shouldBoost = boostColorDetected || 
                         (currentTime - lastBoostColorTime < BOOST_DURATION_AFTER_RED);
      
      if (shouldBoost) {
        // Only apply boost when moving forward
        if (motor_drive > 0) {
          int boostedDrive = motor_drive + BOOST_SPEED_ADD;
          // Limit to maximum boost speed
          motor_drive = min(boostedDrive, MAX_BOOST_SPEED);
          
          // Debug output when boost is active
          static bool lastBoostState = false;
          if (!lastBoostState) {
            Serial.println("BOOST ACTIVATED! Speed increased");
            
            // Play a boost sound when activated
            if (!isPlaying) {
              playFile(5);  // Assuming sound file #5 is for boost
            }
            
            // Save current LED colors before changing to boost color
            byte currentR = 0, currentG = 0, currentB = 0;
            getColorRGB(currentR, currentG, currentB);
            if (!speedModeChanged) {
              speedModeR = currentR;
              speedModeG = currentG;
              speedModeB = currentB;
              speedModeChanged = true;
            }
            
            // Set LED to bright orange/yellow for boost visual feedback
            if (myGamepad && myGamepad->isConnected()) {
              myGamepad->setColorLED(255, 165, 0);  // Orange boost color
              
              // Add rumble effect for boost activation
              myGamepad->playDualRumble(0 /* delayedStartMs */, 200 /* durationMs */, 0x50 /* weakMagnitude */,
                                        0x80 /* strongMagnitude */);
            }
            
            lastBoostState = true;
          }
        }
      } else {
        // Reset boost state tracking
        static bool lastBoostState = false;
        
        if (lastBoostState) {
          Serial.println("Boost deactivated");
          
          // Restore original LED color
          if (speedModeChanged && myGamepad && myGamepad->isConnected()) {
            myGamepad->setColorLED(speedModeR, speedModeG, speedModeB);
            speedModeChanged = false;
          }
          
          lastBoostState = false;
        }
      }
      
      // Update audio state to handle playing sounds
      updateAudio();
      
      // Simplified LED update with much lower frequency
      if (currentTime - lastLedUpdateTime > LED_UPDATE_INTERVAL || hasColorChanged()) {
        lastLedUpdateTime = currentTime;
        
        // Only do LED updates in a stable state
        if (myGamepad && myGamepad->isConnected() && !controllerConnecting) {
          // Update LED with detected color
          byte r = 0, g = 0, b = 0;
          getColorRGB(r, g, b);  // Get the color from the sensor
          
          // Special handling for white to ensure it's properly displayed
          String currentDetectedColor = getDetectedColor();
          if (currentDetectedColor == "White") {
            r = 255; g = 255; b = 255;  // Force true white values
          }
          
          myGamepad->setColorLED(r, g, b);  // Set LED to detected color
          
          // Reset the color changed flag after updating the LED
          if (hasColorChanged()) {
            resetColorChanged();
            
            // Debug information
            static String lastDetectedColor = "";
            
            if (currentDetectedColor != lastDetectedColor) {
              Serial.print("Color changed to: ");
              Serial.println(currentDetectedColor);
              lastDetectedColor = currentDetectedColor;
            }
          }
        }
      }
      
      // Handle button presses for sound playback and calibration
      if (myGamepad && myGamepad->isConnected()) {
        // A/Cross button - play sound 1
        if (myGamepad->a() && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
          playFile(1);
          buttonlastDebounceTime = currentTime;
          Serial.println("A button pressed - playing sound 1");
        }
        
        // B/Circle button - play sound 2
        if (myGamepad->b() && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
          playFile(2);
          buttonlastDebounceTime = currentTime;
          Serial.println("B button pressed - playing sound 2");
        }
        
        // X/Square button - play sound 3
        if (myGamepad->x() && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
          playFile(3);
          buttonlastDebounceTime = currentTime;
          Serial.println("X button pressed - playing sound 3");
        }
        
        // Y/Triangle button - play sound 4
        if (myGamepad->y() && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
          playFile(4);
          buttonlastDebounceTime = currentTime;
          Serial.println("Y button pressed - playing sound 4");
        }
        
        // R1 button (by itself) - play random sound
        if (myGamepad->r1() && !(myGamepad->dpad() & (DPAD_LEFT | DPAD_RIGHT)) && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
          // Get the total number of available sounds (at least 1)
          uint16_t maxSounds = getTotalSoundCount();
          
          // Play a random sound from available files
          uint8_t randomSound = random(1, min(maxSounds + 1, 256)); // Stay within uint8_t range (1-255)
          playFile(randomSound);
          buttonlastDebounceTime = currentTime;
          Serial.print("R1 button pressed - playing random sound #");
          Serial.print(randomSound);
          Serial.print(" of ");
          Serial.println(maxSounds);
        }
        
        // L1 button (by itself) - play sound 7
        if (myGamepad->l1() && !(myGamepad->dpad() & (DPAD_UP | DPAD_DOWN)) && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
          playFile(7);
          buttonlastDebounceTime = currentTime;
          Serial.println("L1 button pressed - playing sound 7");
        }
        
        // L2+R2 buttons - start calibration
        if (myGamepad->l2() && myGamepad->r2() && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
          buttonlastDebounceTime = currentTime;
          Serial.println("L2+R2 pressed - starting color sensor calibration");
          
          // Start the proper color sensor calibration process
          startCalibration();
          
          // Set flag to continue calibration process in the main loop
          calibrationInitiated = true;
          calibrationFinished = false;
        }
        
        // D-pad Up/Down - Adjust color sensor read interval
        if (myGamepad->dpad() & DPAD_UP && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
          // Decrease interval (faster readings)
          if (COLOR_SENSOR_READ_INTERVAL > 10) {
            COLOR_SENSOR_READ_INTERVAL -= 5;
          }
          buttonlastDebounceTime = currentTime;
          Serial.print("Color sensor read interval: ");
          Serial.print(COLOR_SENSOR_READ_INTERVAL);
          Serial.println("ms");
          myGamepad->setColorLED(0, 0, 255);  // Blue flash for feedback
        }
        
        if (myGamepad->dpad() & DPAD_DOWN && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
          // Increase interval (slower readings)
          COLOR_SENSOR_READ_INTERVAL += 5;
          if (COLOR_SENSOR_READ_INTERVAL > 100) {
            COLOR_SENSOR_READ_INTERVAL = 100;  // Cap at 100ms
          }
          buttonlastDebounceTime = currentTime;
          Serial.print("Color sensor read interval: ");
          Serial.print(COLOR_SENSOR_READ_INTERVAL);
          Serial.println("ms");
          myGamepad->setColorLED(255, 255, 0);  // Yellow flash for feedback
        }
        
        // D-pad Left/Right - Adjust red detection sensitivity
        if (myGamepad->dpad() & DPAD_LEFT && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
          // Decrease threshold (more sensitive)
          if (RED_DETECTION_RATIO_THRESHOLD > 1.3) {
            RED_DETECTION_RATIO_THRESHOLD -= 0.1;
          }
          buttonlastDebounceTime = currentTime;
          Serial.print("Red detection threshold: ");
          Serial.println(RED_DETECTION_RATIO_THRESHOLD);
          myGamepad->setColorLED(255, 0, 0);  // Red flash for feedback
        }
        
        if (myGamepad->dpad() & DPAD_RIGHT && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
          // Increase threshold (less sensitive)
          RED_DETECTION_RATIO_THRESHOLD += 0.1;
          if (RED_DETECTION_RATIO_THRESHOLD > 3.0) {
            RED_DETECTION_RATIO_THRESHOLD = 3.0;  // Cap at 3.0
          }
          buttonlastDebounceTime = currentTime;
          Serial.print("Red detection threshold: ");
          Serial.println(RED_DETECTION_RATIO_THRESHOLD);
          myGamepad->setColorLED(255, 128, 0);  // Orange flash for feedback
        }
        
        // Left shoulder (L1) + D-pad Up/Down - Adjust boost speed
        if (myGamepad->l1()) {
          if (myGamepad->dpad() & DPAD_UP && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
            // Increase boost speed
            BOOST_SPEED_ADD += 25;
            if (BOOST_SPEED_ADD > 300) {
              BOOST_SPEED_ADD = 300;  // Cap at 300
            }
            buttonlastDebounceTime = currentTime;
            Serial.print("Boost speed add: ");
            Serial.println(BOOST_SPEED_ADD);
            myGamepad->setColorLED(255, 165, 0);  // Orange flash for feedback
          }
          
          if (myGamepad->dpad() & DPAD_DOWN && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
            // Decrease boost speed
            BOOST_SPEED_ADD -= 25;
            if (BOOST_SPEED_ADD < 50) {
              BOOST_SPEED_ADD = 50;  // Minimum of 50
            }
            buttonlastDebounceTime = currentTime;
            Serial.print("Boost speed add: ");
            Serial.println(BOOST_SPEED_ADD);
            myGamepad->setColorLED(255, 100, 0);  // Dark orange flash for feedback
          }
        }
        
        // Right shoulder (R1) + D-pad Left/Right - Adjust boost duration
        if (myGamepad->r1()) {
          if (myGamepad->dpad() & DPAD_RIGHT && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
            // Increase boost duration
            BOOST_DURATION_AFTER_RED += 250;
            if (BOOST_DURATION_AFTER_RED > 3000) {
              BOOST_DURATION_AFTER_RED = 3000;  // Cap at 3 seconds
            }
            buttonlastDebounceTime = currentTime;
            Serial.print("Boost duration: ");
            Serial.print(BOOST_DURATION_AFTER_RED);
            Serial.println("ms");
            myGamepad->setColorLED(0, 255, 255);  // Cyan flash for feedback
          }
          
          if (myGamepad->dpad() & DPAD_LEFT && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
            // Decrease boost duration
            if (BOOST_DURATION_AFTER_RED >= 250) {
              BOOST_DURATION_AFTER_RED -= 250;
            }
            buttonlastDebounceTime = currentTime;
            Serial.print("Boost duration: ");
            Serial.print(BOOST_DURATION_AFTER_RED);
            Serial.println("ms");
            myGamepad->setColorLED(0, 128, 255);  // Blue-cyan flash for feedback
          }
        }
        
        // Handle calibration process
        if (calibrationInitiated && !calibrationFinished) {
          // Show visual feedback during calibration - flashing white LED
          if ((currentTime / 250) % 2 == 0) {
            myGamepad->setColorLED(255, 255, 255);  // White
          } else {
            myGamepad->setColorLED(0, 0, 0);    // Off
          }
          
          // Process calibration
          if (updateCalibration()) {
            // Calibration is complete when updateCalibration returns true
            calibrationFinished = true;
            calibrationEndTime = currentTime;
            Serial.println("Calibration process completed successfully");
            
            // Visual feedback for calibration complete - solid white
            myGamepad->setColorLED(255, 255, 255);  // White
          }
          
          // Prevent too frequent controller updates during calibration
          if (currentTime - lastControllerUpdateTime > CONTROLLER_UPDATE_INTERVAL) {
            lastControllerUpdateTime = currentTime;
          }
        }
        
        // Reset calibration after showing success for a few seconds
        if (calibrationFinished && currentTime - calibrationEndTime > 3000) {
          calibrationInitiated = false;
          calibrationFinished = false;
        }
      }
      
      // Send motor commands
      Send(motor_steer, -motor_drive);
    } else {
      // No gamepad - send neutral values
      Send(0, 0);
    }
    
    // Ensure regular yield to other tasks
    vTaskDelay(10);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println("Hoverboard Serial v1.0");
  
  // Initialize random number generator with a random seed from an unused analog pin
  randomSeed(analogRead(A0));
  
  // Initialize SPIFFS for calibration data storage
  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS");
  } else {
    Serial.println("SPIFFS initialized successfully");
  }
  
  // Configure the watchdog first with long timeout
  Serial.println("Setting up watchdog timer...");
  esp_task_wdt_init(30, true);  // 30 second timeout during setup
  esp_task_wdt_add(NULL);
  
  // Set up the controller with our minimal handler
  Serial.println("Setting up Bluepad32...");
  BP32.setup(&onConnectedGamepadModified, &onDisconnectedGamepad);
  delay(100);
  
  Serial.println("Setting up other components...");
  setupGamepadController();
  setupAudio();
  
  // Initialize color sensor last
  Serial.println("Setting up color sensor...");
  setupColorSensor();  
  // Start the color sensor task
  startColorSensorTask();
  Serial.println("Color sensor task started");

  // Print initial color sensor settings
  Serial.println("Color sensor settings:");
  Serial.print("  Read interval: ");
  Serial.print(COLOR_SENSOR_READ_INTERVAL);
  Serial.println("ms");
  Serial.print("  Task interval: ");
  Serial.print(COLOR_SENSOR_TASK_INTERVAL);
  Serial.println("ms");
  Serial.print("  Red detection threshold: ");
  Serial.println(RED_DETECTION_RATIO_THRESHOLD);
  Serial.print("  Color intensity threshold: ");
  Serial.println(COLOR_INTENSITY_THRESHOLD);
  Serial.println("Use D-pad to adjust these settings in real-time:");
  Serial.println("  Up/Down: Adjust read interval");
  Serial.println("  Left/Right: Adjust red detection sensitivity");
  
  // Print boost settings
  Serial.println("Boost settings:");
  Serial.print("  Boost speed add: ");
  Serial.println(BOOST_SPEED_ADD);
  Serial.print("  Boost duration after red: ");
  Serial.print(BOOST_DURATION_AFTER_RED);
  Serial.println("ms");
  Serial.println("Use shoulder buttons + D-pad to adjust boost settings:");
  Serial.println("  L1 + Up/Down: Adjust boost speed");
  Serial.println("  R1 + Left/Right: Adjust boost duration");
  
  // Sound controls info
  Serial.println("Button controls for sounds:");
  Serial.println("  A: Play sound 1");
  Serial.println("  B: Play sound 2");
  Serial.println("  X: Play sound 3");
  Serial.println("  Y: Play sound 4");
  Serial.println("  R1: Play a random sound");
  Serial.println("  L1: Play sound 7");

  Serial.println("Setting up serial communication with motors...");
  HoverSerial.begin(HOVER_SERIAL_BAUD, SERIAL_8N1, RXD1, TXD1);
  Serial.println("Serial Txd is on pin: " + String(TXD1));
  Serial.println("Serial Rxd is on pin: " + String(RXD1));

  Serial.println("Creating controller task...");
  // Create a task for controller & motor communication that runs on Core 1
  xTaskCreatePinnedToCore(
    controllerTask,       // Function to implement the task
    "ControllerTask",     // Name of the task
    8192,                 // Stack size in words
    NULL,                 // Task input parameter
    3,                    // Priority (higher than color sensor)
    &controllerTaskHandle,// Task handle
    1);                   // Core 1

  // Reduce timeout to normal after initialization
  Serial.println("Setup complete, reducing watchdog timeout");
  esp_task_wdt_init(10, true);
  
  Serial.println("System ready!");
}

void loop() {
  // Main loop is just for housekeeping
  esp_task_wdt_reset();
  
  // Complete connection if needed
  if (connectionPending) {
    completeConnectionSetup();
  }
  
  // Safety check - if a task is hanging, enable safe mode
  static unsigned long lastLoopCounter = 0;
  static unsigned long checkTime = 0;
  
  if (millis() - checkTime > 3000) {
    if (loopCounter == lastLoopCounter && !controllerConnecting) {
      // Controller task is stuck
      Serial.println("WARNING: Controller task appears to be stuck, entering safe mode");
      enterSafeMode = true;
    }
    
    lastLoopCounter = loopCounter;
    checkTime = millis();
  }
  
  delay(50);
}