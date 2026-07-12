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
  delay(dePin < 0 ? 25 : 8);

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

// Lightweight presence probe. Sends a read-holding request for 1 register and
// classifies the reply:
//   0 = no/invalid response (wrong baud, wiring, or nobody there)
//   1 = normal read reply (device present AND register readable)
//   2 = Modbus exception reply (device present at this baud/slave, but this
//       register/function was rejected -> baud & slave are CORRECT)
inline int modbusProbeRegister(
  HardwareSerial& ser, int dePin, uint8_t slave,
  uint16_t startReg, uint32_t timeoutMs = 350) {
  uint8_t req[8];
  req[0] = slave;
  req[1] = 0x03;
  req[2] = (uint8_t)(startReg >> 8);
  req[3] = (uint8_t)(startReg & 0xFF);
  req[4] = 0x00;
  req[5] = 0x01;
  uint16_t crc = bmsCrcModbus(req, 6);
  req[6] = (uint8_t)(crc & 0xFF);
  req[7] = (uint8_t)(crc >> 8);

  while (ser.available()) ser.read();
  modbusSetTx(dePin, true);
  ser.write(req, 8);
  ser.flush();
  modbusSetTx(dePin, false);
  modbusPump();
  delay(dePin < 0 ? 25 : 2);

  uint8_t hdr[2];
  if (!modbusReadBytes(ser, hdr, 2, timeoutMs)) return 0;
  if (hdr[0] != slave) return 0;
  if (hdr[1] == 0x03) {
    uint8_t rest[132];
    modbusReadBytes(ser, rest, 4, timeoutMs);  // drain byte-count + data + crc
    return 1;
  }
  if (hdr[1] == 0x83) {
    uint8_t rest[3];
    modbusReadBytes(ser, rest, 3, timeoutMs);  // exception code + crc
    return 2;
  }
  return 0;
}

inline bool modbusWriteSingle(
  HardwareSerial& ser, int dePin, uint8_t slave,
  uint16_t reg, uint16_t value,
  uint32_t timeoutMs = 1200,
  uint8_t* exceptionOut = nullptr) {
  if (exceptionOut) *exceptionOut = 0;

  uint8_t req[8];
  req[0] = slave;
  req[1] = 0x06;
  req[2] = (uint8_t)(reg >> 8);
  req[3] = (uint8_t)(reg & 0xFF);
  req[4] = (uint8_t)(value >> 8);
  req[5] = (uint8_t)(value & 0xFF);
  uint16_t crc = bmsCrcModbus(req, 6);
  req[6] = (uint8_t)(crc & 0xFF);
  req[7] = (uint8_t)(crc >> 8);

  while (ser.available()) ser.read();
  modbusSetTx(dePin, true);
  ser.write(req, 8);
  ser.flush();
  modbusSetTx(dePin, false);
  modbusPump();
  delay(dePin < 0 ? 25 : 15);

  uint8_t hdr[2];
  if (!modbusReadBytes(ser, hdr, 2, timeoutMs)) return false;
  if (hdr[0] != slave) return false;

  if (hdr[1] == 0x86) {
    uint8_t tail[3];
    if (!modbusReadBytes(ser, tail, 3, timeoutMs)) return false;
    if (exceptionOut) *exceptionOut = tail[0];
    return false;
  }
  if (hdr[1] != 0x06) return false;

  uint8_t rest[6];
  if (!modbusReadBytes(ser, rest, 6, timeoutMs)) return false;

  uint16_t rxCrc = rest[4] | ((uint16_t)rest[5] << 8);
  uint8_t check[6];
  check[0] = hdr[0];
  check[1] = hdr[1];
  memcpy(check + 2, rest, 4);
  if (bmsCrcModbus(check, 6) != rxCrc) return false;
  uint16_t wReg = ((uint16_t)rest[0] << 8) | rest[1];
  uint16_t wVal = ((uint16_t)rest[2] << 8) | rest[3];
  return wReg == reg && wVal == value;
}

// Cummins docs use 4xxxx (5-digit) or 40xxxx (6-digit) holding register numbers.
// 5-digit: wire = reg - 40001.  6-digit: wire = reg - 400001 (same wire as 5-digit form).
inline uint16_t modbusHoldAddr(uint32_t reg40001) {
  if (reg40001 >= 400001u) return (uint16_t)(reg40001 - 400001u);
  return (uint16_t)(reg40001 - 40001u);
}

inline void modbusBusGap(uint16_t ms = 10) {
  modbusPump();
  delay(ms);
}

inline bool modbusBaudValid(uint32_t baud) {
  return baud == 9600 || baud == 19200 || baud == 38400 || baud == 57600 || baud == 115200;
}
