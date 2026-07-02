#pragma once

#include <Arduino.h>
#include "bms_common.h"

typedef void (*ModbusPumpFn)();

inline ModbusPumpFn& modbusPumpFn() {
  static ModbusPumpFn fn = nullptr;
  return fn;
}

inline void modbusSetPump(ModbusPumpFn fn) {
  modbusPumpFn() = fn;
}

inline void modbusPump() {
  if (modbusPumpFn()) modbusPumpFn()();
}

#ifndef MODBUS_RX_PIN
#define MODBUS_RX_PIN 16
#endif
#ifndef MODBUS_TX_PIN
#define MODBUS_TX_PIN 17
#endif
#ifndef MODBUS_DE_PIN
#define MODBUS_DE_PIN 19
#endif

inline void modbusSetTx(int dePin, bool tx) {
  if (dePin >= 0) digitalWrite(dePin, tx ? HIGH : LOW);
}

inline bool modbusReadBytes(HardwareSerial& ser, uint8_t* buf, size_t len, uint32_t timeoutMs) {
  uint32_t start = millis();
  size_t got = 0;
  while (got < len && millis() - start < timeoutMs) {
    modbusPump();
    while (ser.available()) {
      buf[got++] = (uint8_t)ser.read();
      if (got >= len) return true;
      start = millis();
    }
    delay(1);
  }
  return got >= len;
}

inline bool modbusReadHolding(
  HardwareSerial& ser, int dePin, uint8_t slave,
  uint16_t startReg, uint16_t count, uint16_t* out,
  uint32_t timeoutMs = 1200) {
  if (!count || count > 64) return false;

  uint8_t req[8];
  req[0] = slave;
  req[1] = 0x03;
  req[2] = (uint8_t)(startReg >> 8);
  req[3] = (uint8_t)(startReg & 0xFF);
  req[4] = (uint8_t)(count >> 8);
  req[5] = (uint8_t)(count & 0xFF);
  uint16_t crc = bmsCrcModbus(req, 6);
  req[6] = (uint8_t)(crc & 0xFF);
  req[7] = (uint8_t)(crc >> 8);

  while (ser.available()) ser.read();
  modbusSetTx(dePin, true);
  ser.write(req, 8);
  ser.flush();
  modbusSetTx(dePin, false);
  modbusPump();
  delay(1);

  uint8_t hdr[3];
  if (!modbusReadBytes(ser, hdr, 3, timeoutMs)) return false;
  if (hdr[0] != slave || hdr[1] != 0x03) return false;
  if (hdr[2] != count * 2) return false;

  uint8_t data[128];
  size_t dataLen = hdr[2];
  if (dataLen > sizeof(data)) return false;
  if (!modbusReadBytes(ser, data, dataLen, timeoutMs)) return false;

  uint8_t crcBuf[2];
  if (!modbusReadBytes(ser, crcBuf, 2, timeoutMs)) return false;
  uint16_t rxCrc = crcBuf[0] | ((uint16_t)crcBuf[1] << 8);

  uint8_t check[130];
  check[0] = hdr[0];
  check[1] = hdr[1];
  check[2] = hdr[2];
  memcpy(check + 3, data, dataLen);
  if (bmsCrcModbus(check, dataLen + 3) != rxCrc) return false;

  for (uint16_t i = 0; i < count; i++) {
    out[i] = ((uint16_t)data[i * 2] << 8) | data[i * 2 + 1];
  }
  return true;
}

// Cummins docs use 4xxxx holding register numbers.
inline uint16_t modbusHoldAddr(uint16_t reg40001) {
  return (uint16_t)(reg40001 - 40001);
}
