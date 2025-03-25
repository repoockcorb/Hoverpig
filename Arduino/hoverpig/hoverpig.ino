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
const int BOOST_SPEED_ADD = 120;                       // Fixed value to add during boost
const int MAX_BOOST_SPEED = -450;                     // Maximum allowed speed during boost
const unsigned long BOOST_DURATION_AFTER_RED = 500;  // How long boost continues after leaving red (ms)
unsigned long lastBoostColorTime = 0;                 // Track when we last saw a boost color

// LED update timing
const unsigned long LED_UPDATE_INTERVAL = 80;  // Reduced frequency for more responsive updates
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
const unsigned long CONTROLLER_UPDATE_INTERVAL = 100;  // Update controller at most every 100ms during calibration

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
      vTaskDelay(20);  // Just wait with longer delay
      esp_task_wdt_reset();
      continue;
    }

    // Controller polling - careful not to call too frequently
    if (currentTime - lastControllerPoll >= 20) {  // Poll at 50Hz maximum
      BP32.update();                               // This is the critical call that can block
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
      bool shouldBoost = boostColorDetected || (currentTime - lastBoostColorTime < BOOST_DURATION_AFTER_RED);

      if (shouldBoost) {
        // Only apply boost when moving forward
        if (motor_drive < 0) {
          // Apply boost and limit to maximum speed
          // motor_drive = min(motor_drive - BOOST_SPEED_ADD, MAX_BOOST_SPEED);
          motor_drive = motor_drive - BOOST_SPEED_ADD;
          // if (motor_drive >= MAX_BOOST_SPEED) {
          //   motor_drive = MAX_BOOST_SPEED;
          // }

          // Debug output and effects when boost first activates
          static bool lastBoostState = false;
          if (!lastBoostState) {
            Serial.println("BOOST ACTIVATED!");

            // Play a boost sound when activated
            // if (!isPlaying) {
            //   playFile(5);  // Sound #5 for boost
            // }

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
              myGamepad->setColorLED(255, 165, 0);            // Orange boost color
              myGamepad->playDualRumble(0, 200, 0x50, 0x80);  // Add rumble effect
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
            r = 255;
            g = 255;
            b = 255;  // Force true white values
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

        // R1 button (by itself) - play random sound
        if (myGamepad->r1() && currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
          // Get the total number of available sounds (at least 1)
          uint16_t maxSounds = getTotalSoundCount();

          // Play a random sound from available files
          uint8_t randomSound = random(1, min(maxSounds + 1, 256));  // Stay within uint8_t range (1-255)
          playFile(randomSound);
          buttonlastDebounceTime = currentTime;
          Serial.print("R1 button pressed - playing random sound #");
          Serial.print(randomSound);
          Serial.print(" of ");
          Serial.println(maxSounds);
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

        // Handle calibration process
        if (calibrationInitiated && !calibrationFinished) {
          // Show visual feedback during calibration - flashing white LED
          if ((currentTime / 250) % 2 == 0) {
            myGamepad->setColorLED(255, 255, 255);  // White
          } else {
            myGamepad->setColorLED(0, 0, 0);  // Off
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


      // Reverse Limiter
      if (motor_drive >= 70) {
        motor_drive = 70;
        if (motor_steer >= 70) {
          motor_steer = 70;
        }
        if (motor_steer <= -70) {
          motor_steer = -70;
        }
      } else {
        motor_drive = motor_drive;
        motor_steer = motor_steer;
      }

      // Send motor commands
      Send(motor_steer, -motor_drive);
      delay(5);
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
  Serial.print("  Red threshold: ");
  Serial.println(RED_DETECTION_RATIO_THRESHOLD);

  // Print boost settings
  Serial.println("Boost settings:");
  Serial.print("  Speed add: ");
  Serial.println(BOOST_SPEED_ADD);
  Serial.print("  Duration: ");
  Serial.print(BOOST_DURATION_AFTER_RED);
  Serial.println("ms");

  // Sound controls info
  Serial.println("Button controls:");
  Serial.println("  A/B/X/Y: Play sounds 1-4");
  Serial.println("  R1: Random sound");
  Serial.println("  L1: Sound 7");
  Serial.println("  L2+R2: Calibration");

  Serial.println("Setting up serial communication with motors...");
  HoverSerial.begin(HOVER_SERIAL_BAUD, SERIAL_8N1, RXD1, TXD1);
  Serial.println("Serial Txd is on pin: " + String(TXD1));
  Serial.println("Serial Rxd is on pin: " + String(RXD1));

  Serial.println("Creating controller task...");
  // Create a task for controller & motor communication that runs on Core 1
  xTaskCreatePinnedToCore(
    controllerTask,         // Function to implement the task
    "ControllerTask",       // Name of the task
    8192,                   // Stack size in words
    NULL,                   // Task input parameter
    3,                      // Priority (higher than color sensor)
    &controllerTaskHandle,  // Task handle
    1);                     // Core 1

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