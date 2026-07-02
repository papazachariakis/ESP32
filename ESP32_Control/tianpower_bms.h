#pragma once

#include "bms_common.h"

#define TIANPOWER_SERVICE_UUID "0000ff00-0000-1000-8000-00805f9b34fb"
#define TIANPOWER_CHAR_RX_UUID "0000ff01-0000-1000-8000-00805f9b34fb"
#define TIANPOWER_CHAR_TX_UUID "0000ff02-0000-1000-8000-00805f9b34fb"

#define TIANPOWER_FRAME_SW_VERSION  0x81
#define TIANPOWER_FRAME_HW_VERSION  0x82
#define TIANPOWER_FRAME_STATUS      0x83
#define TIANPOWER_FRAME_GENERAL     0x84
#define TIANPOWER_FRAME_MOSFET      0x85
#define TIANPOWER_FRAME_TEMPS       0x87
#define TIANPOWER_FRAME_CELLS_1_8   0x88
#define TIANPOWER_FRAME_CELLS_9_16  0x89

inline bool tpIsBasenDevice(const String& name) {
  return name.startsWith("TP_");
}

inline void tpBuildRequest(uint8_t frameType, uint8_t out[4]) {
  out[0] = 0x55;
  out[1] = 0x04;
  out[2] = frameType;
  out[3] = 0xAA;
}

inline String tpNullString(const uint8_t* d, int start, int end) {
  String s;
  for (int i = start; i < end; i++) {
    if (d[i] == 0) break;
    s += (char)d[i];
  }
  return s;
}

inline bool tpBit(uint16_t v, uint8_t bit) {
  return (v >> bit) & 1;
}

inline bool tpParseFrame(const uint8_t* data, size_t len, BmsData& bms) {
  if (len < 20 || data[0] != 0x55 || data[1] != 0x14 || data[len - 1] != 0xAA) {
    return false;
  }

  uint8_t type = data[2];

  if (type == TIANPOWER_FRAME_SW_VERSION) {
    bms.swVersion = tpNullString(data, 3, (int)len - 1);
    bms.valid = true;
    bms.lastUpdate = millis();
    return true;
  }

  if (type == TIANPOWER_FRAME_HW_VERSION) {
    bms.deviceModel = tpNullString(data, 3, (int)len - 1);
    bms.valid = true;
    bms.lastUpdate = millis();
    return true;
  }

  if (type == TIANPOWER_FRAME_STATUS) {
    bms.soc = bmsU16BE(data, 3);
    bms.voltage = bmsU16BE(data, 5) / 100.0f;
    bms.avgTemp = bmsS16BE(data, 7) / 10.0f;
    bms.ambientTemp = bmsS16BE(data, 9) / 10.0f;
    bms.mosfetTemp = bmsS16BE(data, 11) / 10.0f;
    bms.current = bmsS16BE(data, 13) / 100.0f;
    bms.power = bms.voltage * bms.current;
    bms.chargePower = bms.power > 0 ? bms.power : 0;
    bms.dischargePower = bms.power < 0 ? -bms.power : 0;
    bms.soh = bmsU16BE(data, 17);
    bms.valid = true;
    bms.lastUpdate = millis();
    return true;
  }

  if (type == TIANPOWER_FRAME_GENERAL) {
    bms.cellCount = data[3];
    bms.tempSensorCount = data[4];
    bms.capacityAh = bmsU16BE(data, 5) / 100.0f;
    bms.remainingAh = bmsU16BE(data, 7) / 100.0f;
    bms.cycles = bmsU16BE(data, 9);
    bms.voltageProtMask = bmsU16BE(data, 11);
    bms.currentProtMask = bmsU16BE(data, 13);
    bms.tempProtMask = bmsU16BE(data, 15);
    bms.errorMask = bmsU16BE(data, 17);
    bms.valid = true;
    bms.lastUpdate = millis();
    return true;
  }

  if (type == TIANPOWER_FRAME_MOSFET) {
    uint16_t mos = bmsU16BE(data, 3);
    bms.charging = tpBit(mos, 1);
    bms.discharging = tpBit(mos, 2);
    bms.limitingCurrent = tpBit(mos, 16) || tpBit(mos, 32);
    uint16_t highAlarm = bmsU16BE(data, 9);
    uint16_t lowAlarm = bmsU16BE(data, 11);
    bms.alarmMask = ((uint32_t)highAlarm << 16) | lowAlarm;
    bms.balancingMask = bmsU16BE(data, 13);
    bms.balancing = bms.balancingMask != 0;
    for (int i = 0; i < 16; i++) {
      bms.cellBalancing[i] = (bms.balancingMask >> i) & 1;
    }
    bms.valid = true;
    bms.lastUpdate = millis();
    return true;
  }

  if (type == TIANPOWER_FRAME_TEMPS) {
    for (int i = 0; i < 8; i++) {
      bms.temps[i] = bmsS16BE(data, 3 + i * 2) / 10.0f;
    }
    bms.valid = true;
    bms.lastUpdate = millis();
    return true;
  }

  if (type == TIANPOWER_FRAME_CELLS_1_8 || type == TIANPOWER_FRAME_CELLS_9_16) {
    int base = (type == TIANPOWER_FRAME_CELLS_1_8) ? 0 : 8;
    for (int i = 0; i < 8; i++) {
      bms.cellVoltages[base + i] = bmsU16BE(data, 3 + i * 2) / 1000.0f;
    }
    bmsUpdateCellStats(bms);
    bms.valid = true;
    bms.lastUpdate = millis();
    return true;
  }

  return false;
}
