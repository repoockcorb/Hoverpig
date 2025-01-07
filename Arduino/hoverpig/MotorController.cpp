#include "MotorController.h"

// Initialize global variables
HardwareSerial HoverSerial(1);
uint8_t idx = 0;
uint16_t bufStartFrame;
byte *p;
byte incomingByte;
byte incomingBytePrev;
SerialCommand Command;
SerialFeedback Feedback;
SerialFeedback NewFeedback;
int val = 0;

// void setup() {
//   Serial.begin(SERIAL_BAUD);
//   Serial.println("Hoverboard Serial v1.0");

//   HoverSerial.begin(HOVER_SERIAL_BAUD, SERIAL_8N1, RXD1, TXD1);  // Specify RX and TX pins for UART1
//   Serial.println("Serial Txd is on pin: " + String(TXD1));
//   Serial.println("Serial Rxd is on pin: " + String(RXD1));
// }

// ########################## SEND ##########################
void Send(int16_t uSteer, int16_t uSpeed)
{
  // Create command
  Command.start    = (uint16_t)START_FRAME;
  Command.steer    = (int16_t)uSteer;
  Command.speed    = (int16_t)uSpeed;
  Command.checksum = (uint16_t)(Command.start ^ Command.steer ^ Command.speed);

  // Write to Serial
  HoverSerial.write((uint8_t *) &Command, sizeof(Command)); 
}

// ########################## RECEIVE ##########################
void Receive()
{
    // Check for new data availability in the Serial buffer
    if (HoverSerial.available()) {
        incomingByte 	  = HoverSerial.read();                                   // Read the incoming byte
        bufStartFrame	= ((uint16_t)(incomingByte) << 8) | incomingBytePrev;       // Construct the start frame
    }
    else {
        return;
    }

  // If DEBUG_RX is defined print all incoming bytes
  #ifdef DEBUG_RX
        Serial.print(incomingByte);
        return;
    #endif

    // Copy received data
    if (bufStartFrame == START_FRAME) {	                    // Initialize if new data is detected
        p       = (byte *)&NewFeedback;
        *p++    = incomingBytePrev;
        *p++    = incomingByte;
        idx     = 2;	
    } else if (idx >= 2 && idx < sizeof(SerialFeedback)) {  // Save the new received data
        *p++    = incomingByte; 
        idx++;
    }	
    
    // Check if we reached the end of the package
    if (idx == sizeof(SerialFeedback)) {
        uint16_t checksum;
        checksum = (uint16_t)(NewFeedback.start ^ NewFeedback.cmd1 ^ NewFeedback.cmd2 ^ NewFeedback.speedR_meas ^ NewFeedback.speedL_meas
                            ^ NewFeedback.batVoltage ^ NewFeedback.boardTemp ^ NewFeedback.cmdLed);

        // Check validity of the new data
        if (NewFeedback.start == START_FRAME && checksum == NewFeedback.checksum) {
            // Copy the new data
            memcpy(&Feedback, &NewFeedback, sizeof(SerialFeedback));

            // Print data to built-in Serial
            Serial.print("1: ");   Serial.print(Feedback.cmd1);
            Serial.print(" 2: ");  Serial.print(Feedback.cmd2);
            Serial.print(" 3: ");  Serial.print(Feedback.speedR_meas);
            Serial.print(" 4: ");  Serial.print(Feedback.speedL_meas);
            Serial.print(" 5: ");  Serial.print(Feedback.batVoltage);
            Serial.print(" 6: ");  Serial.print(Feedback.boardTemp);
            Serial.print(" 7: ");  Serial.println(Feedback.cmdLed);
        } else {
          Serial.println("Non-valid data skipped");
        }
        idx = 0;    // Reset the index (it prevents entering this if condition in the next cycle)
    }

    // Update previous states
    incomingBytePrev = incomingByte;
}











// #include "MotorController.h"

// void setup() {

//   Serial.begin(SERIAL_BAUD);
//   Serial.println("Hoverboard Serial v1.0");

//   HoverSerial.begin(HOVER_SERIAL_BAUD, SERIAL_8N1, RXD1, TXD1);  // Specify RX and TX pins for UART1
//   Serial.println("Serial Txd is on pin: "+String(TXD1));
//   Serial.println("Serial Rxd is on pin: "+String(RXD1));

// }

// // ########################## SEND ##########################
// void Send(int16_t uSteer, int16_t uSpeed)
// {
//   // Create command
//   Command.start    = (uint16_t)START_FRAME;
//   Command.steer    = (int16_t)uSteer;
//   Command.speed    = (int16_t)uSpeed;
//   Command.checksum = (uint16_t)(Command.start ^ Command.steer ^ Command.speed);

//   // Write to Serial
//   HoverSerial.write((uint8_t *) &Command, sizeof(Command)); 
// }

// // ########################## RECEIVE ##########################
// void Receive()
// {
//     // Check for new data availability in the Serial buffer
//     if (HoverSerial.available()) {
//         incomingByte 	  = HoverSerial.read();                                   // Read the incoming byte
//         bufStartFrame	= ((uint16_t)(incomingByte) << 8) | incomingBytePrev;       // Construct the start frame
//     }
//     else {
//         return;
//     }

//   // If DEBUG_RX is defined print all incoming bytes
//   #ifdef DEBUG_RX
//         Serial.print(incomingByte);
//         return;
//     #endif

//     // Copy received data
//     if (bufStartFrame == START_FRAME) {	                    // Initialize if new data is detected
//         p       = (byte *)&NewFeedback;
//         *p++    = incomingBytePrev;
//         *p++    = incomingByte;
//         idx     = 2;	
//     } else if (idx >= 2 && idx < sizeof(SerialFeedback)) {  // Save the new received data
//         *p++    = incomingByte; 
//         idx++;
//     }	
    
//     // Check if we reached the end of the package
//     if (idx == sizeof(SerialFeedback)) {
//         uint16_t checksum;
//         checksum = (uint16_t)(NewFeedback.start ^ NewFeedback.cmd1 ^ NewFeedback.cmd2 ^ NewFeedback.speedR_meas ^ NewFeedback.speedL_meas
//                             ^ NewFeedback.batVoltage ^ NewFeedback.boardTemp ^ NewFeedback.cmdLed);

//         // Check validity of the new data
//         if (NewFeedback.start == START_FRAME && checksum == NewFeedback.checksum) {
//             // Copy the new data
//             memcpy(&Feedback, &NewFeedback, sizeof(SerialFeedback));

//             // Print data to built-in Serial
//             Serial.print("1: ");   Serial.print(Feedback.cmd1);
//             Serial.print(" 2: ");  Serial.print(Feedback.cmd2);
//             Serial.print(" 3: ");  Serial.print(Feedback.speedR_meas);
//             Serial.print(" 4: ");  Serial.print(Feedback.speedL_meas);
//             Serial.print(" 5: ");  Serial.print(Feedback.batVoltage);
//             Serial.print(" 6: ");  Serial.print(Feedback.boardTemp);
//             Serial.print(" 7: ");  Serial.println(Feedback.cmdLed);
//         } else {
//           Serial.println("Non-valid data skipped");
//         }
//         idx = 0;    // Reset the index (it prevents entering this if condition in the next cycle)
//     }

//     // Update previous states
//     incomingBytePrev = incomingByte;
// }