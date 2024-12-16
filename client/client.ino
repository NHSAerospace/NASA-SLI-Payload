#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"
#include <Adafruit_BNO08x.h>
#include <Adafruit_ADXL345_U.h>
#include <Wire.h>
#include "SparkFun_Qwiic_OpenLog_Arduino_Library.h"
#include <SPI.h>
#include <LoRa.h>

// BMP - Altimeter
// ADXL - Accelerometer
// BNO - Orientation

#define SEALEVELPRESSURE_HPA (1013.25) // TODO
Adafruit_BMP3XX bmp;

#define BNO08X_CS 10
#define BNO08X_INT 9
#define BNO08X_RESET -1
Adafruit_BNO08x bno(BNO08X_RESET);
sh2_SensorValue_t sensorValue;

Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(12345);

OpenLog myLog;

unsigned long lastTransmitTime = 0;

void setup()
{
  Serial.begin(9600);
  while (!Serial)
    ;

  Wire.begin();

  // bmp
  if (!bmp.begin_I2C())
  {
    Serial.println("Could not find a valid BMP390 sensor.");
    while (true)
      ;
  }
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);

  // bno
  if (!bno.begin_I2C())
  {
    Serial.println("Could not find a valid BNO085 sensor.");
    while (true)
      ;
  }
  bno.enableReport(SH2_GAME_ROTATION_VECTOR);

  // adxl
  if (!adxl.begin())
  {
    Serial.println("Could not find a valid ADXL345 sensor.");
    while (true)
      ;
  }
  adxl.setRange(ADXL345_RANGE_16_G);

  Serial.println("Sensors initialized successfully.");

  // SD
  myLog.begin();
  Serial.println("SD initialized successfully.");

  myLog.println("Temperature,Pressure,Altitude,ADXL_X,ADXL_Y,ADXL_Z,BNO_I,BNO_J,BNO_K,BNO_REAL");
  myLog.syncFile();

  // LoRa
  if (!LoRa.begin(915E6)) {
    Serial.println("Starting LoRa failed.");
    while (true);
  }
  Serial.println("LoRa initialized successfully.");
}

void loop()
{
  bmp.performReading();
  sensors_event_t adxlEvent;
  adxl.getEvent(&adxlEvent);

  if (bno.getSensorEvent(&sensorValue))
  {
    // Formatting data into string
    String data = String(bmp.temperature) + "," + String(bmp.pressure) + "," + String(bmp.readAltitude(SEALEVELPRESSURE_HPA)) + "," +
                  String(adxlEvent.acceleration.x) + "," + String(adxlEvent.acceleration.y) + "," +
                  String(adxlEvent.acceleration.z) + "," + String(sensorValue.un.gameRotationVector.i) + "," +
                  String(sensorValue.un.gameRotationVector.j) + "," + String(sensorValue.un.gameRotationVector.k) + "," +
                  String(sensorValue.un.gameRotationVector.real);

    myLog.println(data);
    myLog.syncFile();

    Serial.println(data);

    // Transmit data every 2 seconds
    if (millis() - lastTransmitTime >= 2000) {
      LoRa.beginPacket();
      LoRa.print(data);
      LoRa.endPacket();
      lastTransmitTime = millis();
    }
  }

  delay(1000);
}
