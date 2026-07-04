#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>

enum class BmsType : uint8_t {
  None = 0,
  Jk,
};

struct BmsData {
  bool valid = false;
  bool connected = false;
  BmsType type = BmsType::None;
  String name;
  String mac;
  String swVersion;
  String deviceModel;
  float soc = 0;
  float voltage = 0;
  float current = 0;
  float power = 0;
  float chargePower = 0;
  float dischargePower = 0;
  float soh = 0;
  float avgTemp = 0;
  float ambientTemp = 0;
  float mosfetTemp = 0;
  float capacityAh = 0;
  float remainingAh = 0;
  float cycleChargeAh = 0;
  int cellCount = 0;
  int tempSensorCount = 0;
  int cycles = 0;
  float cellVoltages[16] = { 0 };
  float temps[8] = { 0 };
  bool cellBalancing[16] = { false };
  bool charging = false;
  bool discharging = false;
  bool limitingCurrent = false;
  bool balancing = false;
  uint16_t voltageProtMask = 0;
  uint16_t currentProtMask = 0;
  uint16_t tempProtMask = 0;
  uint16_t errorMask = 0;
  uint32_t alarmMask = 0;
  uint16_t balancingMask = 0;
  float minCellV = 0;
  float maxCellV = 0;
  float deltaCellV = 0;
  float avgCellV = 0;
  int minCellNum = 0;
  int maxCellNum = 0;
  unsigned long lastUpdate = 0;
};

inline const char* bmsTypeId(BmsType t) {
  switch (t) {
    case BmsType::Jk: return "jk";
    default: return "unknown";
  }
}

inline const char* bmsTypeLabel(BmsType t) {
  switch (t) {
    case BmsType::Jk: return "JK / Jikong";
    default: return "Unknown BMS";
  }
}

inline BmsType bmsTypeFromString(const String& s) {
  if (s == "jk" || s == "jikong") return BmsType::Jk;
  return BmsType::None;
}

inline BmsType bmsDetectFromName(const String& name) {
  if (name.indexOf("JK") >= 0 || name.indexOf("jk") >= 0) return BmsType::Jk;
  if (name.startsWith("JK_") || name.startsWith("JK-")) return BmsType::Jk;
  return BmsType::None;
}

inline bool bmsMfgMatch(BLEAdvertisedDevice& d, uint16_t id) {
  if (!d.haveManufacturerData()) return false;
  String m = d.getManufacturerData();
  if (m.length() < 2) return false;
  uint16_t cid = (uint8_t)m[0] | ((uint8_t)m[1] << 8);
  return cid == id;
}

inline bool bmsHasService(BLEAdvertisedDevice& d, const char* uuid16) {
  if (!d.haveServiceUUID()) return false;
  return d.isAdvertisingService(BLEUUID(uuid16));
}

inline BmsType bmsDetectType(const String& name, BLEAdvertisedDevice& d) {
  BmsType byName = bmsDetectFromName(name);
  if (byName != BmsType::None) return byName;

  if (bmsHasService(d, "0000ffe0-0000-1000-8000-00805f9b34fb")) {
    return BmsType::Jk;
  }

  if ((bmsMfgMatch(d, 0x0B65) || bmsMfgMatch(d, 0x4B4A)) &&
      (name.indexOf("JK") >= 0 || name.indexOf("jk") >= 0 || name.length() == 0)) {
    return BmsType::Jk;
  }

  return BmsType::None;
}

inline uint16_t bmsU16BE(const uint8_t* d, int o) {
  return (uint16_t(d[o]) << 8) | d[o + 1];
}

inline int16_t bmsS16BE(const uint8_t* d, int o) {
  return (int16_t)bmsU16BE(d, o);
}

inline uint16_t bmsU16LE(const uint8_t* d, int o) {
  return d[o] | (uint16_t(d[o + 1]) << 8);
}

inline int16_t bmsS16LE(const uint8_t* d, int o) {
  return (int16_t)bmsU16LE(d, o);
}

inline uint32_t bmsU32LE(const uint8_t* d, int o) {
  return d[o] | (uint32_t(d[o + 1]) << 8) | (uint32_t(d[o + 2]) << 16) | (uint32_t(d[o + 3]) << 24);
}

inline int32_t bmsS32LE(const uint8_t* d, int o) {
  return (int32_t)bmsU32LE(d, o);
}

inline uint8_t bmsCrcSum(const uint8_t* data, size_t len) {
  uint8_t s = 0;
  for (size_t i = 0; i < len; i++) s += data[i];
  return s;
}

inline uint16_t bmsCrcModbus(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc >> 1) ^ (crc & 1 ? 0xA001 : 0);
    }
  }
  return crc;
}

inline void bmsUpdateCellStats(BmsData& bms) {
  float minV = 1000, maxV = 0, sum = 0;
  int count = 0, minN = 0, maxN = 0;
  for (int i = 0; i < 16; i++) {
    float v = bms.cellVoltages[i];
    if (v <= 0.5f) continue;
    count++;
    sum += v;
    if (v < minV) { minV = v; minN = i + 1; }
    if (v > maxV) { maxV = v; maxN = i + 1; }
  }
  if (count > 0) {
    bms.cellCount = count;
    bms.minCellV = minV;
    bms.maxCellV = maxV;
    bms.deltaCellV = maxV - minV;
    bms.avgCellV = sum / count;
    bms.minCellNum = minN;
    bms.maxCellNum = maxN;
  }
}

inline void bmsFillJson(JsonObject& o, const BmsData& b) {
  o["valid"] = b.valid;
  o["connected"] = b.connected;
  o["type"] = bmsTypeId(b.type);
  o["type_label"] = bmsTypeLabel(b.type);
  o["name"] = b.name;
  o["mac"] = b.mac;
  o["sw_version"] = b.swVersion;
  o["device_model"] = b.deviceModel;
  o["soc"] = b.soc;
  o["voltage"] = b.voltage;
  o["current"] = b.current;
  o["power"] = b.power;
  o["charge_power"] = b.chargePower;
  o["discharge_power"] = b.dischargePower;
  o["soh"] = b.soh;
  o["avg_temp"] = b.avgTemp;
  o["ambient_temp"] = b.ambientTemp;
  o["mosfet_temp"] = b.mosfetTemp;
  o["capacity_ah"] = b.capacityAh;
  o["remaining_ah"] = b.remainingAh;
  o["cycle_charge_ah"] = b.cycleChargeAh;
  o["cell_count"] = b.cellCount;
  o["temp_sensor_count"] = b.tempSensorCount;
  o["cycles"] = b.cycles;
  o["charging"] = b.charging;
  o["discharging"] = b.discharging;
  o["limiting_current"] = b.limitingCurrent;
  o["balancing"] = b.balancing;
  o["min_cell_v"] = b.minCellV;
  o["max_cell_v"] = b.maxCellV;
  o["delta_cell_v"] = b.deltaCellV;
  o["avg_cell_v"] = b.avgCellV;
  o["min_cell_num"] = b.minCellNum;
  o["max_cell_num"] = b.maxCellNum;
  o["voltage_prot_mask"] = b.voltageProtMask;
  o["current_prot_mask"] = b.currentProtMask;
  o["temp_prot_mask"] = b.tempProtMask;
  o["error_mask"] = b.errorMask;
  o["alarm_mask"] = b.alarmMask;
  o["balancing_mask"] = b.balancingMask;

  JsonArray cells = o.createNestedArray("cells");
  for (int i = 0; i < 16; i++) cells.add(b.cellVoltages[i]);
}

inline String bmsToDisplay(const BmsData& b) {
  if (!b.connected) return "JK: not connected";
  if (!b.valid) return "JK: waiting...";
  String s = "JK BMS\n";
  if (b.name.length()) s += b.name + "\n";
  s += "SOC " + String(b.soc, 0) + "%  " + String(b.voltage, 2) + "V  " + String(b.current, 2) + "A\n";
  if (b.capacityAh > 0) {
    s += String(b.remainingAh, 1) + "/" + String(b.capacityAh, 1) + " Ah\n";
  }
  return s;
}
