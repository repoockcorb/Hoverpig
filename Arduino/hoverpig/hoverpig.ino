// main.ino
#include "GamepadController.h"
#include "AudioPlayer.h"
#include "MotorController.h"
#include <ESP32Servo.h>

#include "esp_system.h"
#include "esp_task_wdt.h"  // Include the watchdog timer library


const int BUTTON_DEBOUNCE_DELAY = 500;     // Debounce delay in milliseconds
unsigned long buttonlastDebounceTime = 0;  // Keeps track of the last time the button was pressed

const int DEADZONE_THRESHOLD = 40;         // Deadzone threshold for the controller sticks

int16_t motor_drive = 0;
int16_t motor_steer = 0;

int motor_speed = 0;
int update_remote_led = 1;

void setup() {
  setupGamepadController();
  setupAudio();

  Serial.begin(SERIAL_BAUD);
  Serial.println("Hoverboard Serial v1.0");

  HoverSerial.begin(HOVER_SERIAL_BAUD, SERIAL_8N1, RXD1, TXD1);  // Specify RX and TX pins for UART1
  Serial.println("Serial Txd is on pin: " + String(TXD1));
  Serial.println("Serial Rxd is on pin: " + String(RXD1));

  esp_task_wdt_init(10, true);  // Timeout in seconds (10 seconds here)
  esp_task_wdt_add(NULL);  // Add the current task to the watchdog
}

void loop() {
  // Serial.print("Free heap memory: ");
  // Serial.println(ESP.getFreeHeap());
  esp_task_wdt_reset();  // Reset the watchdog timer to avoid timeout
  loopGamepadController();

  // Check if the gamepad is connected and valid
  if (myGamepad) {

    // Check specific buttons
    if (myGamepad->a()) {
      Serial.println("A button is pressed");
      loopAudio();  // Call loopAudio only if A button is pressed
      // playCurrentFile();
    } else {
      // Serial.println("A button is not pressed");
    }

    unsigned long timeNow = millis();

    // Check for new received data
    Receive();

    if (myGamepad->b()) {
      // Get the current time
      unsigned long currentTime = millis();
      update_remote_led = 1;

      // Check if enough time has passed since the last button press
      if (currentTime - buttonlastDebounceTime > BUTTON_DEBOUNCE_DELAY) {
        // Update the motor speed
        motor_speed += 1;
        if (motor_speed > 2) {
          motor_speed = 0;
        }

        // Update the last debounce time
        buttonlastDebounceTime = currentTime;
      }
    }

    // Apply deadzone logic to the axis values
    int deadzonedAxisY = abs(axisY) < DEADZONE_THRESHOLD ? 0 : axisY;
    int deadzonedAxisRX = abs(axisRX) < DEADZONE_THRESHOLD ? 0 : axisRX;

    if (motor_speed == 0) {
      motor_drive = map(deadzonedAxisY, -500, 500, -120, 120);
      motor_steer = map(deadzonedAxisRX, -500, 500, -100, 100);
      if (update_remote_led == 1) {
        myGamepad->setColorLED(0, 255, 0);  // Green color
        Serial.print("updated led color");
        update_remote_led = 0;
      }
    } else if (motor_speed == 1) {
      motor_drive = map(deadzonedAxisY, -500, 500, -200, 200);
      motor_steer = map(deadzonedAxisRX, -500, 500, -220, 220);
      if (update_remote_led == 1) {
        myGamepad->setColorLED(0, 0, 255);  // Blue color
        Serial.print("updated led color");
        update_remote_led = 0;
      }
    } else if (motor_speed == 2) {
      motor_drive = map(deadzonedAxisY, -500, 500, -500, 500);
      motor_steer = map(deadzonedAxisRX, -500, 500, -500, 500);
      if (update_remote_led == 1) {
        myGamepad->setColorLED(255, 165, 0);  // Orange color
        Serial.print("updated led color");
        update_remote_led = 0;
      }
    }

    Send(motor_steer, -motor_drive);
  }
}