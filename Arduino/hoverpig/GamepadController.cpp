// GamepadController.cpp
#include "GamepadController.h"

// Define variables for joystick offsets
int16_t leftJoystickXOffset = 0;
int16_t leftJoystickYOffset = 0;
int16_t rightJoystickXOffset = 0;
int16_t rightJoystickYOffset = 0;

int16_t axisX = 0;
int16_t axisY = 0;
int16_t axisRX = 0;
int16_t axisRY = 0;

// Define debounce constants
const unsigned long DEBOUNCE_DELAY = 200;  // Adjust debounce delay as needed

// Define variables for debounce and incremental adjustments
unsigned long lastDebounceTime = 0;
int16_t dpadIncrement = 1;

GamepadPtr myGamepad;

// External reference to controller connection flag
extern volatile bool controllerConnecting;

void setupGamepadController() {
    Serial.begin(115200);

    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount file system");
        return;
    }

    // Load offsets from SPIFFS
    loadOffsets();

    // Note: BP32.setup is now called in hoverpig.ino with our modified handler
}

void loopGamepadController() {
    // This function is no longer needed as BP32.update() is called directly
    // in the controller task. Keeping as a stub for compatibility.
    return;
}

// Simple functions for getting joystick values directly
int16_t getAxisYWithOffset() {
    if (!myGamepad) return 0;
    return myGamepad->axisY() + leftJoystickYOffset;
}

int16_t getAxisRXWithOffset() {
    if (!myGamepad) return 0;
    return myGamepad->axisRX() + rightJoystickXOffset;
}

// Simplified offset loading/saving
void saveOffsets() {
    File file = SPIFFS.open("/offsets.txt", "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }

    file.println(leftJoystickXOffset);
    file.println(leftJoystickYOffset);
    file.println(rightJoystickXOffset);
    file.println(rightJoystickYOffset);
    
    file.close();
}

void loadOffsets() {
    File file = SPIFFS.open("/offsets.txt", "r");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }

    leftJoystickXOffset = file.readStringUntil('\n').toInt();
    leftJoystickYOffset = file.readStringUntil('\n').toInt();
    rightJoystickXOffset = file.readStringUntil('\n').toInt();
    rightJoystickYOffset = file.readStringUntil('\n').toInt();
    
    file.close();
}

void onConnectedGamepad(GamepadPtr gp) {
    myGamepad = gp;
    Serial.println("Original connected callback - not used anymore");
}

void onDisconnectedGamepad(GamepadPtr gp) {
    if (myGamepad == gp) {
        myGamepad = nullptr;
        Serial.println("Gamepad disconnected!");
    }
}

