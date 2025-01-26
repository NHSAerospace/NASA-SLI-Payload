#include <SPI.h>
#include <RH_RF95.h>
#include <RHReliableDatagram.h>

#define RFM95_CS 8
#define RFM95_INT 3
#define RFM95_RST 4
#define RF95_FREQ 434.0

#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 2

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
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    uint8_t from;
    
    if (manager.recvfromAck(buf, &len, &from)) {
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.print("Got message from: 0x");
      Serial.print(from, HEX);
      Serial.print(": ");
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);

      uint8_t data[] = "Acknowledgment received";

      if (manager.sendtoWait(data, sizeof(data), from)) {
        Serial.println("Sent reply");
      } else {
        Serial.println("sendtoWait failed");
      }
      digitalWrite(LED_BUILTIN, LOW);
    } else {
      Serial.println("Receive failed");
    }
  }

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

      Serial.print("Message: ");
      Serial.println(receivedChars);

      manager.sendtoWait((uint8_t*)receivedChars, strlen(receivedChars), CLIENT_ADDRESS);

      delay(50);
    }
  }
}
