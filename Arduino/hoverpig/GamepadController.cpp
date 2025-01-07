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

void setupGamepadController() {
    Serial.begin(115200);

    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount file system");
        return;
    }

    // Load offsets from SPIFFS
    loadOffsets();

    BP32.setup(&onConnectedGamepad, &onDisconnectedGamepad);
}

void loopGamepadController() {
    BP32.update();

    if (myGamepad) {
        String output = "";

        // Check if L1 bumper is pressed for left stick offset adjustment
        if (myGamepad->l1()) {
            adjustLeftStickOffsets();
        }

        // Check if R2 bumper is pressed for right stick offset adjustment
        if (myGamepad->r1()) {
            adjustRightStickOffsets();
        }

        // Append DPad directions to output
        uint8_t dpadState = myGamepad->dpad();
        if (dpadState & DPAD_UP) output += "DPad Up ";
        if (dpadState & DPAD_DOWN) output += "DPad Down ";
        if (dpadState & DPAD_LEFT) output += "DPad Left ";
        if (dpadState & DPAD_RIGHT) output += "DPad Right ";

        // Check button states and append to output
        if (myGamepad->a()) output += "Cross ";
        if (myGamepad->b()) output += "Circle ";
        if (myGamepad->x()) output += "Square ";
        if (myGamepad->y()) output += "Triangle ";
        if (myGamepad->l1()) output += "L1 ";
        if (myGamepad->r1()) output += "R1 ";
        if (myGamepad->l2()) output += "L2 ";
        if (myGamepad->r2()) output += "R2 ";

        // Check joystick and trigger values and apply offsets
        // int16_t axisX = myGamepad->axisX() + leftJoystickXOffset;  // Apply offset to axisX
        // int16_t axisY = myGamepad->axisY() + leftJoystickYOffset;  // Apply offset to axisY
        // int16_t axisRX = myGamepad->axisRX() + rightJoystickXOffset;  // Apply offset to axisRX
        // int16_t axisRY = myGamepad->axisRY() + rightJoystickYOffset;  // Apply offset to axisRY

        // Check joystick and trigger values and apply offsets
        axisX = myGamepad->axisX() + leftJoystickXOffset;  // Apply offset to axisX
        axisY = myGamepad->axisY() + leftJoystickYOffset;  // Apply offset to axisY
        axisRX = myGamepad->axisRX() + rightJoystickXOffset;  // Apply offset to axisRX
        axisRY = myGamepad->axisRY() + rightJoystickYOffset;  // Apply offset to axisRY

        if (axisX || axisY) {
            output += "Left Joystick: X = " + String(axisX) + ", Y = " + String(axisY) + " ";
        }

        if (axisRX || axisRY) {
            output += "Right Joystick: RX = " + String(axisRX) + ", RY = " + String(axisRY) + " ";
        }

        if (myGamepad->brake()) {
            output += "Brake (L2) = " + String(myGamepad->brake()) + " ";
        }

        if (myGamepad->throttle()) {
            output += "Throttle (R2) = " + String(myGamepad->throttle()) + " ";
        }

        // Print output if any button is pressed or any joystick/trigger is moved
        if (output != "") {
            Serial.println(output);
        }
    }
}

void adjustLeftStickOffsets() {
    uint8_t dpadState = myGamepad->dpad();

    // Update left joystick X and Y offsets based on DPad input with debounce
    if (dpadState & DPAD_UP && millis() - lastDebounceTime > DEBOUNCE_DELAY) {
        myGamepad->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */, 0x80 /* weakMagnitude */,
                            0x40 /* strongMagnitude */);
        leftJoystickYOffset -= dpadIncrement;  // Adjust Y offset increment as needed
        saveOffsets();
        Serial.print("Updated Left Joystick Y Offset: ");
        Serial.println(leftJoystickYOffset);
        lastDebounceTime = millis();
    }
    if (dpadState & DPAD_DOWN && millis() - lastDebounceTime > DEBOUNCE_DELAY) {
        myGamepad->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */, 0x80 /* weakMagnitude */,
                            0x40 /* strongMagnitude */);
        leftJoystickYOffset += dpadIncrement;  // Adjust Y offset decrement as needed
        saveOffsets();
        Serial.print("Updated Left Joystick Y Offset: ");
        Serial.println(leftJoystickYOffset);
        lastDebounceTime = millis();
    }
    if (dpadState & DPAD_LEFT && millis() - lastDebounceTime > DEBOUNCE_DELAY) {
        myGamepad->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */, 0x80 /* weakMagnitude */,
                            0x40 /* strongMagnitude */);
        leftJoystickXOffset -= dpadIncrement;  // Adjust X offset decrement as needed
        saveOffsets();
        Serial.print("Updated Left Joystick X Offset: ");
        Serial.println(leftJoystickXOffset);
        lastDebounceTime = millis();
    }
    if (dpadState & DPAD_RIGHT && millis() - lastDebounceTime > DEBOUNCE_DELAY) {
        myGamepad->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */, 0x80 /* weakMagnitude */,
                            0x40 /* strongMagnitude */);
        leftJoystickXOffset += dpadIncrement;  // Adjust X offset increment as needed
        saveOffsets();
        Serial.print("Updated Left Joystick X Offset: ");
        Serial.println(leftJoystickXOffset);
        lastDebounceTime = millis();
    }
}

void adjustRightStickOffsets() {
    uint8_t dpadState = myGamepad->dpad();

    // Update right joystick X and Y offsets based on DPad input with debounce
    if (dpadState & DPAD_UP && millis() - lastDebounceTime > DEBOUNCE_DELAY) {
        myGamepad->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */, 0x80 /* weakMagnitude */,
                            0x40 /* strongMagnitude */);
        rightJoystickYOffset += dpadIncrement;  // Adjust Y offset increment as needed
        saveOffsets();
        Serial.print("Updated Right Joystick Y Offset: ");
        Serial.println(rightJoystickYOffset);
        lastDebounceTime = millis();
    }
    if (dpadState & DPAD_DOWN && millis() - lastDebounceTime > DEBOUNCE_DELAY) {
        myGamepad->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */, 0x80 /* weakMagnitude */,
                            0x40 /* strongMagnitude */);
        rightJoystickYOffset -= dpadIncrement;  // Adjust Y offset decrement as needed
        saveOffsets();
        Serial.print("Updated Right Joystick Y Offset: ");
        Serial.println(rightJoystickYOffset);
        lastDebounceTime = millis();
    }
    if (dpadState & DPAD_LEFT && millis() - lastDebounceTime > DEBOUNCE_DELAY) {
        myGamepad->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */, 0x80 /* weakMagnitude */,
                            0x40 /* strongMagnitude */);
        rightJoystickXOffset -= dpadIncrement;  // Adjust X offset decrement as needed
        saveOffsets();
        Serial.print("Updated Right Joystick X Offset: ");
        Serial.println(rightJoystickXOffset);
        lastDebounceTime = millis();
    }
    if (dpadState & DPAD_RIGHT && millis() - lastDebounceTime > DEBOUNCE_DELAY) {
        myGamepad->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */, 0x80 /* weakMagnitude */,
                            0x40 /* strongMagnitude */);
        rightJoystickXOffset += dpadIncrement;  // Adjust X offset increment as needed
        saveOffsets();
        Serial.print("Updated Right Joystick X Offset: ");
        Serial.println(rightJoystickXOffset);
        lastDebounceTime = millis();
    }
}

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
    Serial.println("Gamepad connected!");

    // Example: Change LED color to green when connected
    if (myGamepad->isConnected()) {
        myGamepad->setColorLED(0, 255, 0);  // Green color
    }
}

void onDisconnectedGamepad(GamepadPtr gp) {
    myGamepad = nullptr;
    Serial.println("Gamepad disconnected!");

    // Example: Change LED color to red when disconnected
    if (myGamepad) {
        myGamepad->setColorLED(255, 0, 0);  // Red color
    }
}

