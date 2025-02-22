// #include <Adafuit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include "SparkFun_Qwiic_OpenLog_Arduino_Library.h"

Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(12345);

OpenLog myLog;

const bool DEBUG = true;
unsigned long timestamp;

void setup() {
  // adxl
  if (!adxl.begin()) {
    while (true);
  }
  adxl.setRange(ADXL345_RANGE_16_G);

  // SD
  myLog.begin();
  Serial.println("SD initialized successfully.");
  
  sensors_event_t adxlEvent;
  adxl.getEvent(&adxlEvent);
}

void loop() {
  sensors_event_t adxlEvent;
  adxl.getEvent(&adxlEvent);
  timestamp = millis();

  char dataBuffer[50];
  snprintf(dataBuffer, sizeof(dataBuffer), "%lu, %.2f, %.2f, %.2f", timestamp, adxlEvent.acceleration.x, adxlEvent.acceleration.y, adxlEvent.acceleration.z);
  myLog.println(dataBuffer);
  myLog.syncFile();
}
