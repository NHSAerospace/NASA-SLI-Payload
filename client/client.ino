#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"
#include <Adafruit_BNO08x.h>
#include <Adafruit_ADXL345_U.h>
#include <Wire.h>
#include "SparkFun_Qwiic_OpenLog_Arduino_Library.h"
#include <SPI.h>
#include <RH_RF95.h>
#include <RHReliableDatagram.h>

const bool DEBUG = true;

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
#define RF95_FREQ 868.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);

#define CLIENT_ADDRESS 88
#define SERVER_ADDRESS 89

RHReliableDatagram manager(rf95, CLIENT_ADDRESS);

// Buffer for formatted data
char dataBuffer[RH_RF95_MAX_MESSAGE_LEN];

// Transmission data structure
struct SensorData {
  unsigned long timestamp;
  float temperature;
  float pressure;
  float altitude;
  float adxl_x, adxl_y, adxl_z;
  float bno_i, bno_j, bno_k, bno_real;
  float current_g, max_g;
  float velocity, max_velocity;
  float battery;
};

SensorData sensorData;

enum Mode {
  SELF_TEST,
  IDLE_1,
  SENSOR_ONLY,
  SENSOR_TRANSMISSION,
  TRANSMISSION_ONLY,
  IDLE_2
};

Mode currentMode = SELF_TEST;

unsigned long lastRecord = 0;
unsigned long lastTransmit = 0;
unsigned long currentTime = 0;

byte failureCount = 0;

// Battery
#define VBATPIN A7
float measuredvbat = 0;

void setup() {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  delay(500);
  if (DEBUG) Serial.begin(9600);

  Wire.begin();

  // bmp
  if (!bmp.begin_I2C()) {
    if (DEBUG) Serial.println("Could not find a valid BMP390 sensor.");
    while (true);
  }
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);

  // bno
  if (!bno.begin_I2C()) {
    if (DEBUG) Serial.println("Could not find a valid BNO085 sensor.");
    while (true);
  }
  bno.enableReport(SH2_GAME_ROTATION_VECTOR);

  // adxl
  if (!adxl.begin()) {
    if (DEBUG) Serial.println("Could not find a valid ADXL345 sensor.");
    while (true);
  }
  adxl.setRange(ADXL345_RANGE_16_G);

  if (DEBUG) Serial.println("Sensors initialized successfully.");

  // SD
  myLog.begin();
  if (DEBUG) Serial.println("SD initialized successfully.");

  myLog.println("Timestamp,Temperature,Pressure,Altitude,ADXL_X,ADXL_Y,ADXL_Z,BNO_I,BNO_J,BNO_K,BNO_REAL,MAX_G,VELOCITY,MAX_VELOCITY,BATTERY,MODE");
  myLog.syncFile();

  // Radio
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (!manager.init()) {
    if (DEBUG) Serial.println("Reliable datagram init failed");
    while (true);
  }
  if (DEBUG) Serial.println("Reliable datagram init OK!");

  if (!rf95.setFrequency(RF95_FREQ)) {
    if (DEBUG) Serial.println("setFrequency failed");
    while (true);
  }
  if (DEBUG) Serial.print("Set Freq to: "); if (DEBUG) Serial.println(RF95_FREQ);

  rf95.setTxPower(23, false);

  // Run self test
  selfTestMode();
}

void loop() {
  currentTime = millis();
  switch(currentMode) {
    case SELF_TEST:
      currentMode = IDLE_1;
      break;

    case IDLE_1:
      if (manager.available()) handleModeChange();
      break;

    case SENSOR_ONLY:
      readSensors();
      logSensors();
      if (manager.available()) handleModeChange();
      break;

    case SENSOR_TRANSMISSION:
      readSensors();
      logSensors();
      transmitData();
      if (manager.available()) handleModeChange();
      break;
    
    case TRANSMISSION_ONLY:
      readSensors();
      transmitData();
      if (manager.available()) handleModeChange();
      break;

    case IDLE_2:
      rf95.sleep();
      delay(1000);
      break;
  }
}

void selfTestMode() {
  Serial.println("Running self-test...");

  // sensorData initialization
  bmp.performReading();
  sensors_event_t adxlEvent;
  adxl.getEvent(&adxlEvent);

  measureBattery();

  // This prevents zero division errors when calculating velocity
  for (byte i = 0; i < 2; i++) {
    if (bno.getSensorEvent(&sensorValue)) {
      sensorData.timestamp = millis();
      sensorData.temperature = bmp.temperature;
      sensorData.pressure = bmp.pressure;
      sensorData.altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);
      sensorData.adxl_x = adxlEvent.acceleration.x;
      sensorData.adxl_y = adxlEvent.acceleration.y;
      sensorData.adxl_z = adxlEvent.acceleration.z;
      sensorData.bno_i = sensorValue.un.gameRotationVector.i;
      sensorData.bno_j = sensorValue.un.gameRotationVector.j;
      sensorData.bno_k = sensorValue.un.gameRotationVector.k;
      sensorData.bno_real = sensorValue.un.gameRotationVector.real;
      sensorData.battery = measuredvbat;
    }
  }

  readSensors();
  if (DEBUG) Serial.println("Successfully read from sensors");
  logSensors();
  if (DEBUG) Serial.println("Successfully logged sensors");
  transmitData();
  if (DEBUG) Serial.println("Successfully transmitted data");
  currentMode = IDLE_1;
  if (DEBUG) Serial.println("Successfully completed self test");
}

void readSensors() {
  bmp.performReading();
  sensors_event_t adxlEvent;
  adxl.getEvent(&adxlEvent);

  if (bno.getSensorEvent(&sensorValue)) {
    sensorData.velocity = (bmp.readAltitude(SEALEVELPRESSURE_HPA) - sensorData.altitude) / ((currentTime - sensorData.timestamp) / 1000.0);
    sensorData.timestamp = currentTime;
    sensorData.temperature = bmp.temperature;
    sensorData.pressure = bmp.pressure;
    sensorData.altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);
    sensorData.adxl_x = adxlEvent.acceleration.x;
    sensorData.adxl_y = adxlEvent.acceleration.y;
    sensorData.adxl_z = adxlEvent.acceleration.z;
    sensorData.bno_i = sensorValue.un.gameRotationVector.i;
    sensorData.bno_j = sensorValue.un.gameRotationVector.j;
    sensorData.bno_k = sensorValue.un.gameRotationVector.k;
    sensorData.bno_real = sensorValue.un.gameRotationVector.real;
    sensorData.current_g = getSumVectorMagnitude(sensorData.adxl_x, sensorData.adxl_y, sensorData.adxl_z) / 9.80665;
    if (sensorData.current_g > sensorData.max_g) sensorData.max_g = sensorData.current_g;
    if (sensorData.velocity > sensorData.max_velocity) sensorData.max_velocity = sensorData.velocity;
    sensorData.battery = measuredvbat;
  }

  measureBattery();
}

float getSumVectorMagnitude(float x, float y, float z) {
  return sqrt(x*x + y*y + z*z);
}

void logSensors() {
  if (currentTime - lastRecord > 50) {
    lastRecord = currentTime;
    snprintf(dataBuffer, RH_RF95_MAX_MESSAGE_LEN, 
             "%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d",
             sensorData.timestamp, 
             sensorData.temperature, 
             sensorData.pressure, 
             sensorData.altitude, 
             sensorData.adxl_x, 
             sensorData.adxl_y, 
             sensorData.adxl_z, 
             sensorData.bno_i, 
             sensorData.bno_j, 
             sensorData.bno_k, 
             sensorData.bno_real,
             sensorData.current_g,
             sensorData.max_g,
             sensorData.velocity,
             sensorData.max_velocity,
             sensorData.battery,
             getMode());
    myLog.println(dataBuffer);
    myLog.syncFile();
    if (DEBUG) Serial.println(dataBuffer);
  }
}

void transmitData() {
  if (currentTime - lastTransmit > 500) {
    lastTransmit = currentTime;
    if (manager.sendtoWait((uint8_t*)&sensorData, sizeof(SensorData), SERVER_ADDRESS)) {
      uint8_t len = sizeof(SensorData);
      uint8_t from;
      if (manager.recvfromAckTimeout((uint8_t*)dataBuffer, &len, 200, &from) && DEBUG) {
        Serial.print("Got reply from 0x");
        Serial.print(from, HEX);
        Serial.print(": ");
        Serial.println(dataBuffer);
      } else {
        if (DEBUG) Serial.println("No reply received");
      }
      failureCount = 0;
    } else {
      if (DEBUG) Serial.println("sendtoWait failed");
      failureCount++;
      if (failureCount >= 3) {
        currentMode = SENSOR_ONLY;
      }
    }
  }
}

void handleModeChange() {
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  uint8_t from;
  if (manager.recvfromAck(buf, &len, &from)) {
    if (len > 0) {
      switch(buf[0]) {
        case 'S': currentMode = SELF_TEST; break;
        case 'I': currentMode = IDLE_1; break;
        case 'O': currentMode = SENSOR_ONLY; break;
        case 'T': currentMode = SENSOR_TRANSMISSION; break;
        case 'N': currentMode = TRANSMISSION_ONLY; break;
        case 'E': currentMode = IDLE_2; break;
        case 'Z':
          sensorData.max_g = 0;
          sensorData.max_velocity = 0;
          break;
      }
      if (DEBUG) Serial.print("Mode changed to: ");
      if (DEBUG) Serial.println((char) buf[0]);
    }
  }
}

void measureBattery(void) {
  measuredvbat = analogRead(VBATPIN);
  measuredvbat *=2;
  measuredvbat *=3.3;
  measuredvbat /= 1024;
}

short getMode() {
  switch (currentMode) {
    case SELF_TEST: return 0;
    case IDLE_1: return 1;
    case SENSOR_ONLY: return 2;
    case SENSOR_TRANSMISSION: return 3;
    case TRANSMISSION_ONLY: return 4;
    case IDLE_2: return 5;
  }
  return '!';
}
