#ifndef MOTORCONTROLLER_H
#define MOTORCONTROLLER_H

#include "HardwareSerial.h"
#include "Arduino.h"

// ########################## DEFINES ##########################
#define HOVER_SERIAL_BAUD   115200      // [-] Baud rate for HoverSerial (used to communicate with the hoverboard)
#define SERIAL_BAUD         115200      // [-] Baud rate for built-in Serial (used for the Serial Monitor)
#define START_FRAME         0xABCD     	// [-] Start frame definition for reliable serial communication
#define TIME_SEND           100         // [ms] Sending time interval
#define SPEED_MAX_TEST      400         // [-] Maximum speed for testing
#define SPEED_STEP          20          // [-] Speed step

#define RXD1 16   // RX pin for UART1
#define TXD1 17   // TX pin for UART1

// Use the hardware serial port
extern HardwareSerial HoverSerial;

// Global variables
extern uint8_t idx;
extern uint16_t bufStartFrame;
extern byte *p;
extern byte incomingByte;
extern byte incomingBytePrev;

typedef struct {
   uint16_t start;
   int16_t  steer;
   int16_t  speed;
   uint16_t checksum;
} SerialCommand;
extern SerialCommand Command;

typedef struct {
   uint16_t start;
   int16_t  cmd1;
   int16_t  cmd2;
   int16_t  speedR_meas;
   int16_t  speedL_meas;
   int16_t  batVoltage;
   int16_t  boardTemp;
   uint16_t cmdLed;
   uint16_t checksum;
} SerialFeedback;
extern SerialFeedback Feedback;
extern SerialFeedback NewFeedback;

extern int val;

void Send(int16_t uSteer, int16_t uSpeed);
void Receive();

#endif // MOTORCONTROLLER_H






// #include "HardwareSerial.h"
// #include "Arduino.h"

// // ########################## DEFINES ##########################
// #define HOVER_SERIAL_BAUD   115200      // [-] Baud rate for HoverSerial (used to communicate with the hoverboard)
// #define SERIAL_BAUD         115200      // [-] Baud rate for built-in Serial (used for the Serial Monitor)
// #define START_FRAME         0xABCD     	// [-] Start frame definition for reliable serial communication
// #define TIME_SEND           100         // [ms] Sending time interval
// #define SPEED_MAX_TEST      400         // [-] Maximum speed for testing
// #define SPEED_STEP          20          // [-] Speed step
// // #define DEBUG_RX                        // [-] Debug received data. Prints all bytes to serial (comment-out to disable)

// /*
//  * There are three serial ports on the ESP known as U0UXD, U1UXD and U2UXD.
//  * 
//  * U0UXD is used to communicate with the ESP32 for programming and during reset/boot.
//  * U1UXD is unused and can be used for your projects. Some boards use this port for SPI Flash access though.
//  * U2UXD is unused and can be used for your projects.
//  * 
// */

// #define RXD1 16   // RX pin for UART1
// #define TXD1 17   // TX pin for UART1

// // Use the hardware serial port
// HardwareSerial HoverSerial(1);  // UART1 on ESP32 (using GPIO 4 as RX and GPIO 5 as TX)

// // Global variables
// uint8_t idx = 0;                        // Index for new data pointer
// uint16_t bufStartFrame;                 // Buffer Start Frame
// byte *p;                                // Pointer declaration for the new received data
// byte incomingByte;
// byte incomingBytePrev;

// typedef struct {
//    uint16_t start;
//    int16_t  steer;
//    int16_t  speed;
//    uint16_t checksum;
// } SerialCommand;
// SerialCommand Command;

// typedef struct {
//    uint16_t start;
//    int16_t  cmd1;
//    int16_t  cmd2;
//    int16_t  speedR_meas;
//    int16_t  speedL_meas;
//    int16_t  batVoltage;
//    int16_t  boardTemp;
//    uint16_t cmdLed;
//    uint16_t checksum;
// } SerialFeedback;
// SerialFeedback Feedback;
// SerialFeedback NewFeedback;

// int val = 0;  // variable to store the value read

// void setup();
// void Send(int16_t uSteer, int16_t uSpeed);
// void Receive();