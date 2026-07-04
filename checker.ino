#include <Wire.h>
#include <Adafruit_PN532.h>
#include <ESP32Servo.h>
#include <string.h>

#define PN532_IRQ    6
#define PN532_RESET  7
#define I2C_SDA_PIN  8
#define I2C_SCL_PIN  9
#define SERVO_PIN 10

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);
Servo servo;

uint8_t keyA[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

uint32_t u32(uint8_t *p) {
  return ((uint32_t)p[0]) |
         ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

uint32_t crc32(uint8_t *data, int len) {
  uint32_t crc = 0xFFFFFFFF;

  for (int i = 0; i < len; i++) {
    crc ^= data[i];

    for (int j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xEDB88320;  // fixed
      } else {
        crc >>= 1;
      }
    }
  }

  return ~crc;
}

bool checksumOk(uint8_t *record) {
  uint32_t storedCrc = u32(record + 18);
  uint32_t computedCrc = crc32(record, 18);

  if (storedCrc != computedCrc) {
    Serial.println("Bad CRC");
    return false;
  }

  return true;
}

bool checkCardBalance(uint8_t *uid, uint8_t uidLength) {
  uint8_t block16[16];
  uint8_t block17[16];
  uint8_t record[32];

  if (!nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 16, 0, keyA)) {
    Serial.println("Auth failed");
    return false;
  }

  if (!nfc.mifareclassic_ReadDataBlock(16, block16)) {
    Serial.println("Read block 16 failed");
    return false;
  }

  if (!nfc.mifareclassic_ReadDataBlock(17, block17)) {
    Serial.println("Read block 17 failed");
    return false;
  }

  memcpy(record, block16, 16);
  memcpy(record + 16, block17, 16);

  if (memcmp(record, "GREYGachaCard", 13) != 0) {
    Serial.println("Bad marker");
    return false;
  }

  if (record[13] != 0x01) {
    Serial.println("Bad version");
    return false;
  }

  uint32_t balance = u32(record + 14);

  Serial.print("Balance: ");
  Serial.println(balance);

  if (!checksumOk(record)) {
    return false;
  }

  if (balance < 670000) {
    Serial.println("Insufficient balance");
    return false;
  }

  return true;
}

void startServo() {
  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  
  Serial.println("Resetting Servo ...");
  servo.write(20);
  delay(2000);
}

void startNfc() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();

  if (!versiondata) {
    Serial.println("Could not find PN532. Check wiring and I2C mode.");
    while (1) {
      delay(10);
    }
  }
  Serial.println("Found PN532!");
  nfc.SAMConfig();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  startServo();
  startNfc();
}

void loop() {
  uint8_t uid[7];
  uint8_t uidLength;

  bool success = nfc.readPassiveTargetID(
    PN532_MIFARE_ISO14443A,
    uid,
    &uidLength,
    100
  );

  if (success) {
    Serial.println("NFC tag detected!");
    Serial.print("UID: ");
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) {
        Serial.print("0");
      }
      Serial.print(uid[i], HEX);
      Serial.print(" ");
    }

    Serial.println();

    if (checkCardBalance(uid, uidLength)) {
      Serial.println("Card valid. Moving servo.");
      servo.write(140);
      delay(6767);
      servo.write(20);
      delay(1000);
    } else {
      Serial.println("Card rejected.");
    }
    delay(2000);
  }
}