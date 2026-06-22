#include "BluetoothSerial.h"
#include <Wire.h>

#define STM32_I2C_ADDR 0x08 
#define I2C_RETRY_COUNT 25
#define I2C_RETRY_DELAY_MS 4

BluetoothSerial SerialBT;

/*
 * Transmit data via I2C with retries in case the receiver is busy
 */
static byte transmitWithRetry(const uint8_t *payload, size_t length) {
  byte status = 4;

  for (uint8_t attempt = 0; attempt < I2C_RETRY_COUNT; ++attempt) {
    Wire.beginTransmission(STM32_I2C_ADDR);
    Wire.write(payload, length);
    status = Wire.endTransmission();
    if (status == 0) {
      return 0;
    }
    delay(I2C_RETRY_DELAY_MS);
  }

  return status;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); 
  Wire.setClock(100000);
  SerialBT.begin("ESP32_Bridge_Logger"); 
  Serial.println("ESP32 Master Bridge ready");
}

void loop() {
  while (SerialBT.available()) {
    uint8_t c = SerialBT.read();
    byte err = transmitWithRetry(&c, 1);
    if (err != 0) {
      Serial.print("[I2C Error Tx]: status=");
      Serial.println(err);
    } else {
      Serial.print("[Sent to STM32]: ");
      Serial.write(c);
      Serial.println();
    }
  }
  delay(5);
}
