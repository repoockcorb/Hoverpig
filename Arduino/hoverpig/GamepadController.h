// GamepadController.h
#ifndef GAMEPADCONTROLLER_H
#define GAMEPADCONTROLLER_H

#include <Bluepad32.h>
#include <SPIFFS.h>

// Define variables for joystick offsets
extern int16_t leftJoystickXOffset;
extern int16_t leftJoystickYOffset;
extern int16_t rightJoystickXOffset;
extern int16_t rightJoystickYOffset;

extern int16_t axisX;
extern int16_t axisY;
extern int16_t axisRX;
extern int16_t axisRY;

extern GamepadPtr myGamepad;

// Function declarations
void setupGamepadController();
void loopGamepadController();
void saveOffsets();
void loadOffsets();
void adjustLeftStickOffsets();
void adjustRightStickOffsets();

// Callback function declarations
void onConnectedGamepad(GamepadPtr gp);
void onDisconnectedGamepad(GamepadPtr gp);

#endif // GAMEPADCONTROLLER_H
