#pragma once

#include "bms_common.h"

#define JK_SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define JK_CHAR_UUID    "0000ffe1-0000-1000-8000-00805f9b34fb"

// JK02 BLE cell-info / device-info frames are a fixed 300 bytes.
#define JK_FRAME_LEN 300

struct BmsProtoState {
  uint8_t jkBuf[360];
  size_t jkLen = 0;
  int jkSwVer = 19;
  bool jk32S = false;
  uint32_t cellFrames = 0;
  uint32_t infoFrames = 0;
  uint32_t crcErrors = 0;
  uint8_t basenPollIdx = 0;
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

inline void jkDrop(BmsProtoState& st, size_t n) {
  if (n >= st.jkLen) { st.jkLen = 0; return; }
  memmove(st.jkBuf, st.jkBuf + n, st.jkLen - n);
  st.jkLen -= n;
}

// Keep the buffer aligned so byte 0 is a plausible frame header (0x55 0xAA 0xEB 0x90).
inline void jkResync(BmsProtoState& st) {
  while (st.jkLen >= 1 && st.jkBuf[0] != 0x55) jkDrop(st, 1);
  while (st.jkLen >= 2 && st.jkBuf[1] != 0xAA) {
    jkDrop(st, 1);
    while (st.jkLen >= 1 && st.jkBuf[0] != 0x55) jkDrop(st, 1);
  }
}

// Robust reassembly: BLE delivers 300-byte frames as ~15 notifications that may
// desync. Accumulate, resync to the header, validate CRC over the fixed frame
// length, parse, then consume exactly one frame. Returns true if a cell-info
// (0x02) frame was successfully parsed.
inline bool jkFeed(BmsProtoState& st, const uint8_t* data, size_t len, BmsData& bms) {
  bool gotCell = false;
  for (size_t i = 0; i < len; i++) {
    if (st.jkLen >= sizeof(st.jkBuf)) jkDrop(st, 1);
    st.jkBuf[st.jkLen++] = data[i];
    jkResync(st);

    while (st.jkLen >= JK_FRAME_LEN &&
           st.jkBuf[0] == 0x55 && st.jkBuf[1] == 0xAA &&
           st.jkBuf[2] == 0xEB && st.jkBuf[3] == 0x90) {
      if (bmsCrcSum(st.jkBuf, JK_FRAME_LEN - 1) == st.jkBuf[JK_FRAME_LEN - 1]) {
        uint8_t frameType = st.jkBuf[4];
        if (jkParseFrame(st.jkBuf, JK_FRAME_LEN, bms, st)) {
          if (frameType == 0x02) { gotCell = true; st.cellFrames++; }
          else if (frameType == 0x03) st.infoFrames++;
        }
        jkDrop(st, JK_FRAME_LEN);
      } else {
        st.crcErrors++;
        jkDrop(st, 1);
        jkResync(st);
      }
    }
  }
  return gotCell;
}

// --- Basen Green / Tianpower BLE (service FF00, notify FF01, write FF02) ---
#define BASEN_SERVICE_UUID "0000ff00-0000-1000-8000-00805f9b34fb"
#define BASEN_NOTIFY_UUID  "0000ff01-0000-1000-8000-00805f9b34fb"
#define BASEN_WRITE_UUID   "0000ff02-0000-1000-8000-00805f9b34fb"
#define BASEN_FRAME_LEN 20

static const uint8_t BASEN_POLL_CMDS[] = {
  0x83, 0x84, 0x85, 0x87, 0x88, 0x89, 0x8A,
};

inline size_t basenBuildCmd(uint8_t frameType, uint8_t* out) {
  out[0] = 0x55;
  out[1] = 0x04;
  out[2] = frameType;
  out[3] = 0xAA;
  return 4;
}

inline bool basenParseFrame(const uint8_t* f, size_t len, BmsData& bms, BmsProtoState& st, bool& gotCell) {
  if (len != BASEN_FRAME_LEN || f[0] != 0x55 || f[1] != 0x14 || f[19] != 0xAA) return false;
  gotCell = false;
  uint8_t frameType = f[2];

  switch (frameType) {
    case 0x81: {
      bms.swVersion = "";
      for (int i = 3; i < 19; i++) {
        if (f[i] == 0) break;
        bms.swVersion += (char)f[i];
      }
      bms.valid = true;
      bms.lastUpdate = millis();
      st.infoFrames++;
      return true;
    }
    case 0x82: {
      bms.deviceModel = "";
      for (int i = 3; i < 19; i++) {
        if (f[i] == 0) break;
        bms.deviceModel += (char)f[i];
      }
      bms.valid = true;
      bms.lastUpdate = millis();
      st.infoFrames++;
      return true;
    }
    case 0x83: {
      bms.soc = (float)bmsU16BE(f, 3);
      bms.voltage = bmsU16BE(f, 5) * 0.01f;
      bms.avgTemp = bmsS16BE(f, 7) * 0.1f;
      bms.ambientTemp = bmsS16BE(f, 9) * 0.1f;
      bms.mosfetTemp = bmsS16BE(f, 11) * 0.1f;
      bms.current = bmsS16BE(f, 13) * 0.01f;
      bms.soh = (float)bmsU16BE(f, 17);
      bms.power = bms.voltage * bms.current;
      bms.chargePower = bms.power > 0 ? bms.power : 0;
      bms.dischargePower = bms.power < 0 ? -bms.power : 0;
      if (fabsf(bms.current) > 0.05f) {
        bms.charging = bms.current > 0;
        bms.discharging = bms.current < 0;
      }
      bms.valid = true;
      bms.lastUpdate = millis();
      st.cellFrames++;
      gotCell = true;
      return true;
    }
    case 0x84: {
      uint8_t nCells = f[3];
      bms.cellCount = (nCells > 0 && nCells <= 16) ? nCells : 16;
      bms.tempSensorCount = f[4];
      bms.capacityAh = bmsU16BE(f, 5) * 0.01f;
      bms.remainingAh = bmsU16BE(f, 7) * 0.01f;
      bms.cycles = (int)bmsU16BE(f, 9);
      bms.voltageProtMask = bmsU16BE(f, 11);
      bms.currentProtMask = bmsU16BE(f, 13);
      bms.tempProtMask = bmsU16BE(f, 15);
      bms.errorMask = bmsU16BE(f, 17);
      bms.valid = true;
      bms.lastUpdate = millis();
      return true;
    }
    case 0x85: {
      uint16_t mos = bmsU16BE(f, 3);
      // MOS status bits can stay set while current is opposite — trust amps when flowing.
      if (fabsf(bms.current) > 0.05f) {
        bms.charging = bms.current > 0;
        bms.discharging = bms.current < 0;
      } else {
        bms.charging = (mos & (1 << 1)) != 0;
        bms.discharging = (mos & (1 << 2)) != 0;
      }
      bms.limitingCurrent = (mos & (1 << 4)) != 0 || (mos & (1 << 5)) != 0;
      bms.balancingMask = bmsU16BE(f, 13);
      bms.balancing = bms.balancingMask != 0;
      for (int i = 0; i < 16; i++) {
        bms.cellBalancing[i] = (bms.balancingMask & (1u << i)) != 0;
      }
      bms.alarmMask = ((uint32_t)bmsU16BE(f, 9) << 16) | bmsU16BE(f, 11);
      bms.valid = true;
      bms.lastUpdate = millis();
      return true;
    }
    case 0x87: {
      int tc = 0;
      float tsum = 0;
      for (int i = 0; i < 8; i++) {
        int16_t raw = bmsS16BE(f, 3 + i * 2);
        if (raw == 0 && i > 1) continue;
        float t = raw * 0.1f;
        if (t < -50 || t > 150) continue;
        if (tc < 8) bms.temps[tc++] = t;
        tsum += t;
      }
      if (tc > 0) {
        bms.tempSensorCount = tc;
        bms.avgTemp = tsum / tc;
      }
      bms.valid = true;
      bms.lastUpdate = millis();
      return true;
    }
    case 0x88:
    case 0x89:
    case 0x8A: {
      uint8_t chunk = frameType - 0x88;
      uint8_t offset = chunk * 8;
      for (int i = 0; i < 8; i++) {
        int idx = offset + i;
        if (idx >= 16) break;
        float cv = bmsU16BE(f, 3 + i * 2) * 0.001f;
        if (cv > 0.5f && cv < 5.0f) bms.cellVoltages[idx] = cv;
      }
      bmsUpdateCellStats(bms);
      bms.valid = true;
      bms.lastUpdate = millis();
      st.cellFrames++;
      gotCell = true;
      return true;
    }
    default:
      return false;
  }
}

inline bool basenFeed(BmsProtoState& st, const uint8_t* data, size_t len, BmsData& bms) {
  bool gotCell = false;
  if (len < BASEN_FRAME_LEN) return false;
  for (size_t i = 0; i + BASEN_FRAME_LEN <= len; i++) {
    bool frameCell = false;
    if (basenParseFrame(data + i, BASEN_FRAME_LEN, bms, st, frameCell) && frameCell) gotCell = true;
  }
  return gotCell;
}
