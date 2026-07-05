#pragma once

#include "bms_common.h"

#define JK_SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define JK_CHAR_UUID    "0000ffe1-0000-1000-8000-00805f9b34fb"

struct BmsProtoState {
  uint8_t jkBuf[336];
  size_t jkLen = 0;
  int jkSwVer = 19;
  bool jk32S = false;
};

inline int jkMajorVersion(const String& v) {
  int dot = v.indexOf('.');
  if (dot > 0) return v.substring(0, dot).toInt();
  return v.toInt();
}

inline bool jkModelIs32S(const String& model) {
  if (model.indexOf("32S") >= 0 || model.indexOf("32s") >= 0) return true;
  if (model.startsWith("JK-PB") || model.startsWith("PB2A")) return true;
  return false;
}

// JK FW 15+ (e.g. 19.07 on B2A8S20P) uses JK02_32S frame layout (+32 byte data offset).
inline bool jkUseExtendedFrame(const BmsProtoState& st) {
  return st.jk32S || st.jkSwVer >= 15;
}

inline int jkDataOff(const BmsProtoState& st) {
  return jkUseExtendedFrame(st) ? 32 : 0;
}

inline int jkBaseOff(const BmsProtoState& st) {
  return jkUseExtendedFrame(st) ? 16 : 0;
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

inline bool jkParseDeviceInfo(const uint8_t* f, size_t len, BmsData& bms, BmsProtoState& st) {
  if (len < 38 || f[0] != 0x55 || f[1] != 0xAA || f[2] != 0xEB || f[3] != 0x90 || f[4] != 0x03) return false;
  if (bmsCrcSum(f, len - 1) != f[len - 1]) return false;

  bms.deviceModel = "";
  for (int i = 6; i < 22 && i < (int)len; i++) {
    if (f[i] == 0) break;
    bms.deviceModel += (char)f[i];
  }
  bms.swVersion = "";
  for (int i = 30; i < 38 && i < (int)len; i++) {
    if (f[i] == 0) break;
    bms.swVersion += (char)f[i];
  }
  if (bms.swVersion.length()) st.jkSwVer = jkMajorVersion(bms.swVersion);
  st.jk32S = jkModelIs32S(bms.deviceModel);
  bms.valid = true;
  bms.lastUpdate = millis();
  return true;
}

inline bool jkParseCellInfo(const uint8_t* f, size_t len, BmsData& bms, const BmsProtoState& st) {
  if (len < 200 || f[0] != 0x55 || f[1] != 0xAA || f[2] != 0xEB || f[3] != 0x90 || f[4] != 0x02) return false;
  if (bmsCrcSum(f, len - 1) != f[len - 1]) return false;

  const int baseOff = jkBaseOff(st);
  const int dataOff = jkDataOff(st);
  const int maxCells = jkUseExtendedFrame(st) ? 32 : 24;

  for (int i = 0; i < maxCells && i < 16; i++) {
    uint16_t cv = bmsU16LE(f, 6 + i * 2);
    if (cv >= 500 && cv <= 5000) {
      bms.cellVoltages[i] = cv / 1000.0f;
    } else if (i < 16) {
      bms.cellVoltages[i] = 0;
    }
  }

  bms.voltage = bmsU32LE(f, 118 + dataOff) / 1000.0f;
  bms.current = bmsS32LE(f, 126 + dataOff) / 1000.0f;
  bms.soc = f[141 + dataOff];
  bms.remainingAh = bmsU32LE(f, 142 + dataOff) / 1000.0f;
  bms.capacityAh = bmsU32LE(f, 146 + dataOff) / 1000.0f;
  bms.cycles = (int)bmsU32LE(f, 150 + dataOff);
  bms.cycleChargeAh = bmsU32LE(f, 154 + dataOff) / 1000.0f;
  bms.soh = f[158 + dataOff];
  bms.balancing = f[140 + dataOff] != 0;

  float tsum = 0;
  int tc = 0;
  auto addTemp = [&](int16_t raw) {
    if (raw == -2000 || raw < -500 || raw > 1500) return;
    float t = raw / 10.0f;
    if (tc < 8) bms.temps[tc++] = t;
    tsum += t;
  };
  addTemp(bmsS16LE(f, 130 + dataOff));
  addTemp(bmsS16LE(f, 132 + dataOff));
  if (jkUseExtendedFrame(st)) {
    bms.mosfetTemp = bmsS16LE(f, 112 + dataOff) / 10.0f;
    addTemp(bmsS16LE(f, 254));
    addTemp(bmsS16LE(f, 256));
    addTemp(bmsS16LE(f, 258));
  } else {
    bms.mosfetTemp = bmsS16LE(f, 134 + dataOff) / 10.0f;
  }

  if (tc > 0) {
    bms.avgTemp = tsum / tc;
    bms.tempSensorCount = tc;
    if (tc > 0) bms.ambientTemp = bms.temps[0];
  }

  bms.errorMask = jkUseExtendedFrame(st)
    ? (uint16_t)(bmsU32LE(f, 134 + dataOff) & 0xFFFF)
    : bmsU16LE(f, 136 + dataOff);
  bms.deltaCellV = bmsU16LE(f, 60 + baseOff) / 1000.0f;

  if (fabsf(bms.current) > 0.05f) {
    bms.charging = bms.current > 0;
    bms.discharging = bms.current < 0;
  } else {
    bms.charging = false;
    bms.discharging = false;
  }

  bms.power = bms.voltage * bms.current;
  bms.chargePower = bms.power > 0 ? bms.power : 0;
  bms.dischargePower = bms.power < 0 ? -bms.power : 0;

  bmsUpdateCellStats(bms);
  bms.valid = true;
  bms.lastUpdate = millis();
  return true;
}

inline bool jkParseFrame(const uint8_t* f, size_t len, BmsData& bms, BmsProtoState& st) {
  if (len < 5 || f[0] != 0x55 || f[1] != 0xAA || f[2] != 0xEB || f[3] != 0x90) return false;
  if (f[4] == 0x03) return jkParseDeviceInfo(f, len, bms, st);
  if (f[4] == 0x02) return jkParseCellInfo(f, len, bms, st);
  return false;
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
      int tryLen = (int)st.jkLen;
      if (tryLen > (int)sizeof(st.jkBuf)) tryLen = (int)sizeof(st.jkBuf);
      for (; tryLen >= 280; tryLen--) {
        if (bmsCrcSum(st.jkBuf, tryLen - 1) != st.jkBuf[tryLen - 1]) continue;
        if (jkParseFrame(st.jkBuf, (size_t)tryLen, bms, st)) {
          uint8_t frameType = st.jkBuf[4];
          st.jkLen = 0;
          return frameType == 0x02;
        }
        break;
      }
    }
    if (st.jkLen >= sizeof(st.jkBuf)) st.jkLen = 0;
  }
  return false;
}
