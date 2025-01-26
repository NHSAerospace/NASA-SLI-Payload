#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"
#include <Adafruit_BNO08x.h>
#include <Adafruit_ADXL345_U.h>
#include <Wire.h>
#include "SparkFun_Qwiic_OpenLog_Arduino_Library.h"
#include <SPI.h>
#include <RH_RF95.h>
#include <RHReliableDatagram.h>

#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BMP3XX bmp;

#define BNO08X_CS 10
#define BNO08X_INT 9
#define BNO08X_RESET -1
Adafruit_BNO08x bno(BNO08X_RESET);
sh2_SensorValue_t sensorValue;

Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(12345);

OpenLog myLog;

// Radio
#define RFM95_CS    8
#define RFM95_INT   3
#define RFM95_RST   4
#define RF95_FREQ 434.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);

#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 2

RHReliableDatagram manager(rf95, CLIENT_ADDRESS);

// Buffer for formatted data
char dataBuffer[RH_RF95_MAX_MESSAGE_LEN];

void setup() {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

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
  bno.enableReport(SH2_GAME_ROTATION_VECTOR);

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

  myLog.println("Temperature,Pressure,Altitude,ADXL_X,ADXL_Y,ADXL_Z,BNO_I,BNO_J,BNO_K,BNO_REAL");
  myLog.syncFile();

  // Radio
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed.");
    while (true);
  }
  Serial.println("LoRa radio init OK!");

  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (true);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

  rf95.setTxPower(23, false);

  if (!manager.init()) {
    Serial.println("Reliable datagram init failed");
    while (true);
  }
  Serial.println("Reliable datagram init OK!");
}

void loop() {
  bmp.performReading();
  sensors_event_t adxlEvent;
  adxl.getEvent(&adxlEvent);

  if (bno.getSensorEvent(&sensorValue)) {
    // Format data into buffer using snprintf
    snprintf(dataBuffer, RH_RF95_MAX_MESSAGE_LEN,
             "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f",
             bmp.temperature,
             bmp.pressure,
             bmp.readAltitude(SEALEVELPRESSURE_HPA),
             adxlEvent.acceleration.x,
             adxlEvent.acceleration.y,
             adxlEvent.acceleration.z,
             sensorValue.un.gameRotationVector.i,
             sensorValue.un.gameRotationVector.j,
             sensorValue.un.gameRotationVector.k,
             sensorValue.un.gameRotationVector.real);

    // Log to SD card
    myLog.println(dataBuffer);
    myLog.syncFile();

    // Print to Serial
    Serial.println(dataBuffer);

    // Send the data using RadioHead
    if (manager.sendtoWait((uint8_t*)dataBuffer, strlen(dataBuffer), SERVER_ADDRESS)) {
      // Wait for a reply from the server
      uint8_t len = sizeof(dataBuffer);
      uint8_t from;
      
      if (manager.recvfromAckTimeout((uint8_t*)dataBuffer, &len, 2000, &from)) {
        Serial.print("Got reply from 0x");
        Serial.print(from, HEX);
        Serial.print(": ");
        Serial.println(dataBuffer);
      } else {
        Serial.println("No reply received");
      }
    } else {
      Serial.println("sendtoWait failed");
    }
  }

  delay(1000);
}
