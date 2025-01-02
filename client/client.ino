#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"
#include <Adafruit_BNO08x.h>
#include <Adafruit_ADXL345_U.h>
#include <Wire.h>
#include "SparkFun_Qwiic_OpenLog_Arduino_Library.h"
#include <SPI.h>
#include <RH_RF95.h>

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

// Radio
#define RFM95_CS    8
#define RFM95_INT   3
#define RFM95_RST   4

#define RF95_FREQ 434.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);

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
    while (true)
      ;
  }
  bno.enableReport(SH2_GAME_ROTATION_VECTOR);

  // adxl
  if (!adxl.begin()) {
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
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed.");
    while (true);
  }
  Serial.println("LoRa initialized successfully.");

  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (true);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

  rf95.setTxPower(23, false);
}

int16_t packetnum = 0;

void loop() {
  bmp.performReading();
  sensors_event_t adxlEvent;
  adxl.getEvent(&adxlEvent);

  if (bno.getSensorEvent(&sensorValue)) {
    // Formatting data into string
    String data = String(bmp.temperature) + "," + String(bmp.pressure) + "," + String(bmp.readAltitude(SEALEVELPRESSURE_HPA)) + "," +
                  String(adxlEvent.acceleration.x) + "," + String(adxlEvent.acceleration.y) + "," +
                  String(adxlEvent.acceleration.z) + "," + String(sensorValue.un.gameRotationVector.i) + "," +
                  String(sensorValue.un.gameRotationVector.j) + "," + String(sensorValue.un.gameRotationVector.k) + "," +
                  String(sensorValue.un.gameRotationVector.real);

    // myLog.println(data);
    // myLog.syncFile();

    Serial.println(data);

    // Transmit data every 2 seconds
    char radiopacket[20] = "Hello World #      ";
    itoa(packetnum++, radiopacket+13, 10);
    Serial.print("Sending "); Serial.println(radiopacket);
    radiopacket[19] = 0;

    Serial.println("Sending...");
    delay(10);
    rf95.send((uint8_t *)radiopacket, 20);

    Serial.println("Waiting for packet to complete...");
    delay(10);
    rf95.waitPacketSent();
    // Now wait for a reply
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    Serial.println("Waiting for reply...");
    if (rf95.waitAvailableTimeout(1000)) {
      // Should be a reply message for us now
      if (rf95.recv(buf, &len)) {
        Serial.print("Got reply: ");
        Serial.println((char*)buf);
        Serial.print("RSSI: ");
        Serial.println(rf95.lastRssi(), DEC);
      } else {
        Serial.println("Receive failed");
      }
    } else {
      Serial.println("No reply, is there a listener around?");
    }
  }
  delay(1000);
}
