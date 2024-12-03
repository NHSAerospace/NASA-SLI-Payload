#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"
#include <Adafruit_BNO08x.h>
#include <Adafruit_ADXL345_U.h>
#include <Wire.h>
#include "SparkFun_Qwiic_OpenLog_Arduino_Library.h"

#define SEALEVELPRESSURE_HPA (1013.25)  // TODO
Adafruit_BMP3XX bmp;

#define BNO08X_CS 10
#define BNO08X_INT 9
#define BNO08X_RESET -1
Adafruit_BNO08x bno(BNO08X_RESET);
sh2_SensorValue_t sensorValue;

Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(12345);

OpenLog myLog;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Wire.begin();

  // bmp
  if (!bmp.begin_I2C()) {
    Serial.println("Could not find a valid BMP390 sensor.");
    while (true);
  }
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);

  // bno
  if (!bno.begin_I2C()) {
    Serial.println("Could not find a valid BNO085 sensor.");
    while (true);
  }

  // adxl
  if (!adxl.begin()) {
    Serial.println("Could not find a valid ADXL345 sensor.");
    while (true);
  }
  adxl.setRange(ADXL345_RANGE_16_G);

  Serial.println("Sensors initialized successfully.");

  // SD
  myLog.begin();
  Serial.println("SD initialized successfully.");

  //Test
  myLog.println("012345678901235467890123456789");
  myLog.syncFile();
  delay(1000);
  Serial.println("Maybe it worked?");
}

void loop() {
  

}
