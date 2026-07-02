#pragma once

#include "bms_common.h"

#define JBD_SERVICE_UUID "0000ff00-0000-1000-8000-00805f9b34fb"
#define JBD_CHAR_RX_UUID "0000ff01-0000-1000-8000-00805f9b34fb"
#define JBD_CHAR_TX_UUID "0000ff02-0000-1000-8000-00805f9b34fb"

#define DALY_SERVICE_UUID "0000fff0-0000-1000-8000-00805f9b34fb"
#define DALY_CHAR_RX_UUID "0000fff1-0000-1000-8000-00805f9b34fb"
#define DALY_CHAR_TX_UUID "0000fff2-0000-1000-8000-00805f9b34fb"

#define JK_SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define JK_CHAR_UUID    "0000ffe1-0000-1000-8000-00805f9b34fb"

#define ANT_SERVICE_UUID JK_SERVICE_UUID
#define ANT_CHAR_UUID    JK_CHAR_UUID

struct BmsProtoState {
  uint8_t jbdBuf[160];
  size_t jbdLen = 0;
  uint8_t jbdExpect = 0;

  uint8_t antBuf[256];
  size_t antLen = 0;
  uint16_t antExpLen = 0;
  uint8_t antValidReply = 0x11;

  uint8_t jkBuf[320];
  size_t jkLen = 0;
  int jkSwVer = 11;
  int jkProtOff = 0;
};

inline uint16_t jbdCrc(const uint8_t* frame, size_t from, size_t to) {
  uint32_t sum = 0;
  for (size_t i = from; i < to; i++) sum += frame[i];
  return (uint16_t)(0x10000 - sum);
}

inline size_t jbdBuildCmd(uint8_t cmd, uint8_t* out) {
  out[0] = 0xDD;
  out[1] = 0xA5;
  out[2] = cmd;
  out[3] = 0x00;
  uint16_t crc = jbdCrc(out, 2, 4);
  out[4] = (crc >> 8) & 0xFF;
  out[5] = crc & 0xFF;
  out[6] = 0x77;
  return 7;
}

inline bool jbdParseBasic(const uint8_t* f, size_t len, BmsData& bms) {
  if (len < 27 || f[0] != 0xDD || f[1] != 0x03 || f[2] != 0x00) return false;
  const uint8_t* d = f + 4;
  bms.voltage = bmsU16BE(d, 0) / 100.0f;
  bms.current = bmsS16BE(d, 2) / 100.0f;
  bms.remainingAh = bmsU16BE(d, 4) / 100.0f;
  bms.capacityAh = bmsU16BE(d, 6) / 100.0f;
  bms.cycles = bmsU16BE(d, 8);
  bms.soc = d[19];
  bms.charging = (d[20] & 0x1) != 0;
  bms.discharging = (d[20] & 0x2) != 0;
  bms.cellCount = d[21];
  bms.tempSensorCount = d[22];
  bms.power = bms.voltage * bms.current;
  bms.chargePower = bms.power > 0 ? bms.power : 0;
  bms.dischargePower = bms.power < 0 ? -bms.power : 0;
  float tsum = 0;
  int tc = 0;
  for (int i = 0; i < bms.tempSensorCount && i < 8; i++) {
    float t = (bmsU16BE(d, 23 + i * 2) - 2731) / 10.0f;
    bms.temps[i] = t;
    tsum += t;
    tc++;
  }
  if (tc > 0) bms.avgTemp = tsum / tc;
  bms.errorMask = bmsU16BE(d, 16);
  bms.valid = true;
  bms.lastUpdate = millis();
  return true;
}

inline bool jbdParseCells(const uint8_t* f, size_t len, BmsData& bms) {
  if (len < 8 || f[0] != 0xDD || f[1] != 0x04 || f[2] != 0x00) return false;
  int cells = f[3] / 2;
  if (cells > 16) cells = 16;
  const uint8_t* d = f + 4;
  for (int i = 0; i < cells; i++) {
    bms.cellVoltages[i] = bmsU16BE(d, i * 2) / 1000.0f;
  }
  bmsUpdateCellStats(bms);
  bms.valid = true;
  bms.lastUpdate = millis();
  return true;
}

inline bool jbdFeed(BmsProtoState& st, const uint8_t* data, size_t len, BmsData& bms) {
  for (size_t i = 0; i < len; i++) {
    uint8_t b = data[i];
    if (st.jbdLen == 0 && b != 0xDD) continue;
    if (st.jbdLen < sizeof(st.jbdBuf)) st.jbdBuf[st.jbdLen++] = b;
    if (st.jbdLen < 7) continue;
    size_t total = 7 + st.jbdBuf[3];
    if (st.jbdLen < total) continue;
    if (st.jbdBuf[total - 1] != 0x77) { st.jbdLen = 0; continue; }
    uint16_t crc = jbdCrc(st.jbdBuf, 2, total - 2);
    if (crc != bmsU16BE(st.jbdBuf, total - 3)) { st.jbdLen = 0; continue; }
    bool ok = false;
    if (st.jbdBuf[1] == 0x03) ok = jbdParseBasic(st.jbdBuf, total, bms);
    else if (st.jbdBuf[1] == 0x04) ok = jbdParseCells(st.jbdBuf, total, bms);
    else if (st.jbdBuf[1] == 0x05) {
      bms.deviceModel = "";
      for (size_t j = 4; j < 4 + st.jbdBuf[3] && j < total - 1; j++) {
        if (st.jbdBuf[j] == 0) break;
        bms.deviceModel += (char)st.jbdBuf[j];
      }
      bms.valid = true;
      ok = true;
    }
    st.jbdLen = 0;
    if (ok) return true;
  }
  return false;
}

inline size_t dalyBuildRead(uint16_t addr, uint16_t count, uint8_t* out) {
  out[0] = 0xD2;
  out[1] = 0x03;
  out[2] = (addr >> 8) & 0xFF;
  out[3] = addr & 0xFF;
  out[4] = (count >> 8) & 0xFF;
  out[5] = count & 0xFF;
  uint16_t crc = bmsCrcModbus(out, 6);
  out[6] = crc & 0xFF;
  out[7] = (crc >> 8) & 0xFF;
  return 8;
}

inline bool dalyParse(const uint8_t* data, size_t len, BmsData& bms) {
  if (len < 10 || data[0] != 0xD2 || data[1] != 0x03) return false;
  size_t dlen = data[2];
  if (len != dlen + 5) return false;
  uint16_t crc = bmsCrcModbus(data, len - 2);
  if (crc != bmsU16LE(data, len - 2)) return false;

  const uint8_t* p = data + 3;
  bms.voltage = bmsU16BE(p, 80) / 10.0f;
  bms.current = (bmsU16BE(p, 82) - 30000) / 10.0f;
  bms.soc = bmsU16BE(p, 84) / 10.0f;
  bms.remainingAh = bmsU16BE(p, 96) / 10.0f;
  bms.cellCount = bmsU16BE(p, 98);
  if (bms.cellCount > 16) bms.cellCount = 16;
  bms.tempSensorCount = bmsU16BE(p, 100);
  if (bms.tempSensorCount > 8) bms.tempSensorCount = 8;
  bms.cycles = bmsU16BE(p, 102);
  bms.deltaCellV = bmsU16BE(p, 112) / 1000.0f;
  bms.charging = bmsU16BE(p, 106) != 0;
  bms.discharging = bmsU16BE(p, 108) != 0;
  bms.balancing = bmsU16BE(p, 104) != 0;
  bms.power = bms.voltage * bms.current;
  bms.chargePower = bms.power > 0 ? bms.power : 0;
  bms.dischargePower = bms.power < 0 ? -bms.power : 0;
  bms.capacityAh = bms.remainingAh > 0 && bms.soc > 0 ? bms.remainingAh * 100.0f / bms.soc : 0;

  for (int i = 0; i < bms.cellCount; i++) {
    bms.cellVoltages[i] = bmsU16BE(p, i * 2) / 1000.0f;
  }
  float tsum = 0;
  int tc = 0;
  for (int i = 0; i < bms.tempSensorCount; i++) {
    float t = bmsU16BE(p, 64 + i * 2) - 40.0f;
    bms.temps[i] = t;
    tsum += t;
    tc++;
  }
  if (tc > 0) bms.avgTemp = tsum / tc;
  bmsUpdateCellStats(bms);
  bms.valid = true;
  bms.lastUpdate = millis();
  return true;
}

inline size_t jkBuildCmd(uint8_t cmd, uint8_t* out) {
  out[0] = 0xAA;
  out[1] = 0x55;
  out[2] = 0x90;
  out[3] = 0xEB;
  out[4] = cmd;
  out[5] = 0x00;
  for (int i = 6; i < 19; i++) out[i] = 0;
  out[19] = bmsCrcSum(out, 19);
  return 20;
}

inline bool jkParseStatus(const uint8_t* f, size_t len, BmsData& bms, int off) {
  if (len < 200 || f[0] != 0x55 || f[1] != 0xAA || f[2] != 0xEB || f[3] != 0x90) return false;
  if (f[4] != 0x02 && f[4] != 0x03) return false;
  if (bmsCrcSum(f, len - 1) != f[len - 1]) return false;

  if (f[4] == 0x03) {
    bms.deviceModel = "";
    for (int i = 6; i < 22; i++) {
      if (f[i] == 0) break;
      bms.deviceModel += (char)f[i];
    }
    bms.swVersion = "";
    for (int i = 30; i < 38; i++) {
      if (f[i] == 0) break;
      bms.swVersion += (char)f[i];
    }
    int maj = 11;
    if (bms.swVersion.length()) maj = bms.swVersion.toInt();
    if (maj < 11) off = -32;
    bms.valid = true;
    bms.lastUpdate = millis();
    return true;
  }

  bms.voltage = bmsU32LE(f, 150 + off) / 1000.0f;
  bms.current = bmsS32LE(f, 158 + off) / 1000.0f;
  bms.soc = f[173 + off];
  bms.remainingAh = bmsU32LE(f, 174 + off) / 1000.0f;
  bms.capacityAh = bmsU32LE(f, 178 + off);
  bms.cycles = bmsU32LE(f, 182 + off);
  bms.soh = f[190 + off];
  bms.charging = f[198 + off] != 0;
  bms.discharging = f[199 + off] != 0;
  bms.balancing = f[172 + off] != 0;
  bms.power = bms.voltage * bms.current;
  bms.chargePower = bms.power > 0 ? bms.power : 0;
  bms.dischargePower = bms.power < 0 ? -bms.power : 0;

  uint32_t cellBits = bmsU32LE(f, 70 + (off >> 1));
  int cells = 0;
  for (int i = 0; i < 32 && cells < 16; i++) {
    if (cellBits & (1u << i)) cells++;
  }
  if (cells == 0) cells = 16;
  for (int i = 0; i < cells && i < 16; i++) {
    bms.cellVoltages[i] = bmsU16LE(f, 6 + i * 2) / 1000.0f;
  }
  bms.deltaCellV = bmsU16LE(f, 76 + (off >> 1)) / 1000.0f;

  int tmask = bmsS16LE(f, 214 + off);
  float tsum = 0;
  int tc = 0;
  int tpos[] = { 144, 162, 164, 254, 256, 258 };
  for (int i = 0; i < 6 && tc < 8; i++) {
    if (!(tmask & (1 << i))) continue;
    int16_t raw = bmsS16LE(f, tpos[i] + off);
    if (raw == -2000) continue;
    float t = raw / 10.0f;
    bms.temps[tc++] = t;
    tsum += t;
  }
  if (tc > 0) bms.avgTemp = tsum / tc;
  bms.tempSensorCount = tc;
  bms.errorMask = bmsU32LE(f, 166 + off) & 0xFFFF;
  bmsUpdateCellStats(bms);
  bms.valid = true;
  bms.lastUpdate = millis();
  return true;
}

inline bool jkFeed(BmsProtoState& st, const uint8_t* data, size_t len, BmsData& bms) {
  for (size_t i = 0; i < len; i++) {
    uint8_t b = data[i];
    if (st.jkLen == 0 && b != 0x55 && b != 0x41) continue;
    if (st.jkLen == 0 && b == 0x41) {
      if (i + 3 < len && data[i + 1] == 0x54 && data[i + 2] == 0x0D && data[i + 3] == 0x0A) {
        i += 3;
        continue;
      }
    }
    if (st.jkLen < sizeof(st.jkBuf)) st.jkBuf[st.jkLen++] = b;
    if (st.jkLen < 5) continue;
    if (st.jkBuf[0] != 0x55 || st.jkBuf[1] != 0xAA) { st.jkLen = 0; continue; }
    if (st.jkLen >= 300) {
      if (jkParseStatus(st.jkBuf, 300, bms, st.jkProtOff)) {
        if (bms.swVersion.length()) {
          int maj = bms.swVersion.toInt();
          st.jkSwVer = maj;
          st.jkProtOff = maj < 11 ? -32 : 0;
        }
        st.jkLen = 0;
        return true;
      }
      st.jkLen = 0;
    }
  }
  return false;
}

inline size_t antBuildCmd(uint8_t cmd, uint16_t addr, uint8_t length, uint8_t* out) {
  out[0] = 0x7E;
  out[1] = 0xA1;
  out[2] = cmd;
  out[3] = addr & 0xFF;
  out[4] = (addr >> 8) & 0xFF;
  out[5] = length;
  uint16_t crc = bmsCrcModbus(out + 1, 5);
  out[6] = crc & 0xFF;
  out[7] = (crc >> 8) & 0xFF;
  out[8] = 0xAA;
  out[9] = 0x55;
  return 10;
}

inline bool antParse(const uint8_t* f, size_t len, BmsData& bms) {
  if (len < 10 || f[0] != 0x7E || f[1] != 0xA1) return false;
  if (f[len - 2] != 0xAA || f[len - 1] != 0x55) return false;
  uint16_t crc = bmsCrcModbus(f + 1, len - 4);
  if (crc != bmsU16LE(f, len - 4)) return false;

  if ((f[2] & 0xF0) == 0x10 && f[2] != 0x12) {
    int cells = f[9];
    if (cells > 16) cells = 16;
    int temps = f[8];
    if (temps > 6) temps = 6;
  int base = 34;
    for (int i = 0; i < cells; i++) {
      bms.cellVoltages[i] = bmsU16LE(f, base + i * 2) / 1000.0f;
    }
    int tbase = base + cells * 2;
    float tsum = 0;
    int tc = 0;
    for (int i = 0; i < temps + 2 && tc < 8; i++) {
      float t = bmsS16LE(f, tbase + i * 2);
      bms.temps[tc++] = t;
      tsum += t;
    }
    if (tc > 0) bms.avgTemp = tsum / tc;
    bms.tempSensorCount = tc;
    int off = (temps + cells) * 2;
    bms.voltage = bmsU16LE(f, 38 + off) / 100.0f;
    bms.current = bmsS16LE(f, 40 + off) / 10.0f;
    bms.soc = bmsU16LE(f, 42 + off);
    bms.soh = bmsU16LE(f, 44 + off);
    bms.capacityAh = bmsU32LE(f, 50 + off);
    bms.remainingAh = bmsU32LE(f, 54 + off) / 1e6f;
    bms.deltaCellV = bmsU16LE(f, 82 + off) / 1000.0f;
    bms.power = bms.voltage * bms.current;
    bms.chargePower = bms.power > 0 ? bms.power : 0;
    bms.dischargePower = bms.power < 0 ? -bms.power : 0;
    bms.charging = f[7] == 0x02;
    bms.cellCount = cells;
    bmsUpdateCellStats(bms);
    bms.valid = true;
    bms.lastUpdate = millis();
    return true;
  }
  if (f[2] == 0x12) {
    bms.swVersion = "";
    for (int i = 22; i < 38; i++) {
      if (f[i] == 0) break;
      bms.swVersion += (char)f[i];
    }
    bms.deviceModel = "";
    for (int i = 6; i < 22; i++) {
      if (f[i] == 0) break;
      bms.deviceModel += (char)f[i];
    }
    bms.valid = true;
    bms.lastUpdate = millis();
    return true;
  }
  return false;
}

inline bool antFeed(BmsProtoState& st, const uint8_t* data, size_t len, BmsData& bms) {
  for (size_t i = 0; i < len; i++) {
    uint8_t b = data[i];
    if (st.antLen == 0 && b != 0x7E) continue;
    if (st.antLen == 0) st.antExpLen = 0;
    if (st.antLen < sizeof(st.antBuf)) st.antBuf[st.antLen++] = b;
    if (st.antLen >= 6 && st.antExpLen == 0) st.antExpLen = st.antBuf[5] + 10;
    if (st.antExpLen > 0 && st.antLen >= st.antExpLen) {
      if (antParse(st.antBuf, st.antLen, bms)) {
        st.antLen = 0;
        st.antExpLen = 0;
        return true;
      }
      st.antLen = 0;
      st.antExpLen = 0;
    }
  }
  return false;
}
