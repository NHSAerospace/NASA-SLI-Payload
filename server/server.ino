#include <SPI.h>
#include <RH_RF95.h>
#include <RHReliableDatagram.h>

#define RFM95_CS 8
#define RFM95_INT 3
#define RFM95_RST 4
#define RF95_FREQ 868.0

#define CLIENT_ADDRESS 88
#define SERVER_ADDRESS 89

RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHReliableDatagram manager(rf95, SERVER_ADDRESS);

// Accepting input
const byte numChars = 32;
char receivedChars[numChars];

enum Mode {
  SELF_TEST,
  IDLE_1,
  SENSEOR_ONLY,
  SENSOR_TRANSMISSION,
  TRANSMISSION_ONLY,
  IDLE_2
};

Mode currentMode = IDLE_1;

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
  Mode mode;
};

// ENABLE DEBUG BY ENTERING "D" INTO SERIAL
bool DEBUG = false;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(9600);
  while (!Serial);

  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (!manager.init()) {
    Serial.println("RHReliableDatagram init failed");
    while (true);
  }
  Serial.println("RHReliableDatagram init OK!");

  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (true);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

  rf95.setTxPower(23, false);
}

void loop() {
  if (manager.available()) {
    uint8_t buf[sizeof(SensorData)];
    uint8_t len = sizeof(buf);
    uint8_t from;
    
    if (manager.recvfromAck(buf, &len, &from)) {
      digitalWrite(LED_BUILTIN, HIGH);
      if (DEBUG) {
        Serial.print("Got message from: 0x");
        Serial.print(from, HEX);
        Serial.print(": ");
      }

      SensorData *receivedData = (SensorData*)buf;
      Serial.print(receivedData->timestamp);
      Serial.print(",");
      Serial.print(receivedData->temperature, 5);
      Serial.print(",");
      Serial.print(receivedData->pressure, 5);
      Serial.print(",");
      Serial.print(receivedData->altitude, 5);
      Serial.print(",");
      Serial.print(receivedData->adxl_x, 5);
      Serial.print(",");
      Serial.print(receivedData->adxl_y, 5);
      Serial.print(",");
      Serial.print(receivedData->adxl_z, 5);
      Serial.print(",");
      Serial.print(receivedData->bno_i, 5);
      Serial.print(",");
      Serial.print(receivedData->bno_j, 5);
      Serial.print(",");
      Serial.print(receivedData->bno_k, 5);
      Serial.print(",");
      Serial.print(receivedData->bno_real, 5);
      Serial.print(",");
      Serial.print(receivedData->current_g, 5);
      Serial.print(",");
      Serial.print(receivedData->max_g, 5);
      Serial.print(",");
      Serial.print(receivedData->velocity, 5);
      Serial.print(",");
      Serial.print(receivedData->max_velocity, 5);
      Serial.print(",");
      Serial.print(receivedData->battery, 5);
      Serial.print(",");
      Serial.print((int)receivedData->mode);
      Serial.print(",");
      Serial.print(rf95.lastRssi(), DEC);
      Serial.print(",");
      Serial.println(rf95.lastSNR(), DEC);

      uint8_t data[] = "Acknowledgment received";

      if (manager.sendtoWait(data, sizeof(data), from)) {
        if (DEBUG) Serial.println("Sent reply");
      } else {
        Serial.println("sendtoWait failed");
      }
      digitalWrite(LED_BUILTIN, LOW);
    } else {
      Serial.println("Receive failed");
    }
  } else {
    // Check for serial input to change mode
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;

    if (Serial.available() > 0) {
      rc = Serial.read();

      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      } else {
        receivedChars[ndx] = '\0';
        ndx = 0;

        if (DEBUG) {
          Serial.print("Message: ");
          Serial.println(receivedChars);
        }

        if (receivedChars[0] == 'D') DEBUG = !DEBUG;
        else manager.sendtoWait((uint8_t*)receivedChars, strlen(receivedChars), CLIENT_ADDRESS);
      }
    }
  }
  delay(50);
}
