#include "Wire.h"
#include "veml6040.h"

VEML6040 sensor;

// Replace these values with your own calibration factors from the calibration routine
float rScale = 0.9650;  // Example: overallAvg / measuredRed
float gScale = 0.7963;  // Example: overallAvg / measuredGreen
float bScale = 1.4125;  // Example: overallAvg / measuredBlue

// Helper function that estimates a color name from calibrated RGB values
String getColorName(float red, float green, float blue) {
  // Calculate the total intensity
  float sum = red + green + blue;
  if (sum == 0) return "None";  // No color detected

  // Normalize the RGB values (range 0.0 - 1.0)
  float r = red / sum;
  float g = green / sum;
  float b = blue / sum;

  // Find max and min for hue calculation
  float maxVal = r;
  if (g > maxVal) maxVal = g;
  if (b > maxVal) maxVal = b;
  
  float minVal = r;
  if (g < minVal) minVal = g;
  if (b < minVal) minVal = b;
  
  float delta = maxVal - minVal;
  float hue = 0;
  
  // Compute hue (in degrees)
  if (delta == 0) {
    hue = 0; // Undefined hue; default to 0
  } else if (maxVal == r) {
    hue = 60 * fmod(((g - b) / delta), 6);
  } else if (maxVal == g) {
    hue = 60 * (((b - r) / delta) + 2);
  } else if (maxVal == b) {
    hue = 60 * (((r - g) / delta) + 4);
  }
  if (hue < 0) hue += 360;  // Ensure hue is positive

  // Map hue to approximate color names.
  if (hue < 15 || hue >= 345) return "Red";
  else if (hue < 45) return "Orange";
  else if (hue < 75) return "Yellow";
  else if (hue < 165) return "Green";
  else if (hue < 195) return "Cyan";
  else if (hue < 255) return "Blue";
  else if (hue < 285) return "Purple";
  else return "Magenta";
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  
  if (!sensor.begin()) {
    Serial.println("ERROR: couldn't detect the sensor");
    while (1) {} // Halt if sensor not detected
  }
  
  // Configure sensor: 320ms integration time, auto mode, sensor enabled
  sensor.setConfiguration(VEML6040_IT_320MS + VEML6040_AF_AUTO + VEML6040_SD_ENABLE);
  
  delay(1500);
  Serial.println("Vishay VEML6040 sensor initialized.");
  Serial.println("Using calibration factors:");
  Serial.print("  rScale: "); Serial.println(rScale);
  Serial.print("  gScale: "); Serial.println(gScale);
  Serial.print("  bScale: "); Serial.println(bScale);
  Serial.println("Starting color detection...");
}

void loop() {
  // Read raw sensor values
  uint16_t rawRed   = sensor.getRed();
  uint16_t rawGreen = sensor.getGreen();
  uint16_t rawBlue  = sensor.getBlue();
  uint16_t rawWhite = sensor.getWhite();

  // Apply calibration factors to the raw values
  float calibratedRed   = rawRed * rScale;
  float calibratedGreen = rawGreen * gScale;
  float calibratedBlue  = rawBlue * bScale;

  // Use calibrated values to determine the most likely color name
  String colorName = getColorName(calibratedRed, calibratedGreen, calibratedBlue);

  // Print raw and calibrated sensor values along with detected color
  Serial.print("Raw - R: ");
  Serial.print(rawRed);
  Serial.print("  G: ");
  Serial.print(rawGreen);
  Serial.print("  B: ");
  Serial.print(rawBlue);
  Serial.print("  W: ");
  Serial.println(rawWhite);

  Serial.print("Calibrated - R: ");
  Serial.print(calibratedRed, 2);
  Serial.print("  G: ");
  Serial.print(calibratedGreen, 2);
  Serial.print("  B: ");
  Serial.print(calibratedBlue, 2);
  Serial.print("  Detected Color: ");
  Serial.println(colorName);

  delay(100);
}













// #include "Wire.h"
// #include "veml6040.h"

// VEML6040 sensor;

// // Number of samples to average for calibration
// const int numSamples = 10;

// void setup() {
//   Serial.begin(9600);
//   Wire.begin();
  
//   if (!sensor.begin()) {
//     Serial.println("ERROR: couldn't detect the sensor");
//     while (1) {} // Halt if sensor not detected
//   }
  
//   // Initialize sensor configuration:
//   //  - 320ms integration time
//   //  - auto mode
//   //  - sensor enabled
//   sensor.setConfiguration(VEML6040_IT_320MS + VEML6040_AF_AUTO + VEML6040_SD_ENABLE);
  
//   delay(1500);
//   Serial.println("White Balance Calibration");
//   Serial.println("=================================");
//   Serial.println("1. Place a white calibration target (e.g., a white card) in front of the sensor.");
//   Serial.println("2. When ready, press any key in the Serial Monitor to start calibration.");
  
//   // Wait for user input
//   while (!Serial.available()) {
//     ; // Wait until data is available
//   }
//   // Clear any received character
//   while(Serial.available()) {
//     Serial.read();
//   }
  
//   Serial.println("Starting calibration...");
  
//   // Variables to accumulate sensor values
//   unsigned long rSum = 0, gSum = 0, bSum = 0;
  
//   // Take several samples for a stable reading
//   for (int i = 0; i < numSamples; i++) {
//     uint16_t r = sensor.getRed();
//     uint16_t g = sensor.getGreen();
//     uint16_t b = sensor.getBlue();
    
//     rSum += r;
//     gSum += g;
//     bSum += b;
    
//     Serial.print("Sample ");
//     Serial.print(i + 1);
//     Serial.print(" - R: ");
//     Serial.print(r);
//     Serial.print("  G: ");
//     Serial.print(g);
//     Serial.print("  B: ");
//     Serial.println(b);
    
//     delay(500); // wait a bit between samples
//   }
  
//   // Compute average values for each channel
//   float rAvg = rSum / (float)numSamples;
//   float gAvg = gSum / (float)numSamples;
//   float bAvg = bSum / (float)numSamples;
  
//   // Calculate the overall average intensity of the three channels.
//   float overallAvg = (rAvg + gAvg + bAvg) / 3.0;
  
//   // Compute scaling factors so that each channel will be balanced to the overall average.
//   float rScale = overallAvg / rAvg;
//   float gScale = overallAvg / gAvg;
//   float bScale = overallAvg / bAvg;
  
//   Serial.println("\nCalibration Complete!");
//   Serial.println("Calibration Factors (White Balance):");
//   Serial.print("  Red scale factor:   ");
//   Serial.println(rScale, 4);
//   Serial.print("  Green scale factor: ");
//   Serial.println(gScale, 4);
//   Serial.print("  Blue scale factor:  ");
//   Serial.println(bScale, 4);
  
//   Serial.println("\nApply these scale factors to your sensor readings in your main code.");
// }

// void loop() {
//   // After calibration, you might choose to do nothing here,
//   // or run your main application code where you use the calibration factors.
// }
