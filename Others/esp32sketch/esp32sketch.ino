#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

static void forwardToStm32(Stream &source, const char *label) {
  while (source.available()) {
    uint8_t c = source.read();
    Serial2.write(c);

    Serial.print("[");
    Serial.print(label);
    Serial.print(" -> STM32]: 0x");
    if (c < 16) {
      Serial.print('0');
    }
    Serial.print(c, HEX);
    Serial.print(" '");
    Serial.write(c);
    Serial.println("'");
  }
}

void setup() {
  Serial.begin(115200);
  // RX = GPIO16, TX = GPIO17
  Serial2.begin(921600, SERIAL_8N1, 16, 17);
  
  SerialBT.begin("ESP32_Bridge_Logger"); 
  Serial.println("ESP32 UART Bridge ready");
}

void loop() {
  forwardToStm32(Serial, "USB");
  forwardToStm32(SerialBT, "BT");
  delay(5);
}
