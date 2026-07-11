#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>

enum class BmsType : uint8_t {
  None = 0,
  Jk,
  Basen,
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
    case BmsType::Basen: return "basen";
    default: return "unknown";
  }
}

inline const char* bmsTypeLabel(BmsType t) {
  switch (t) {
    case BmsType::Jk: return "JK / Jikong";
    case BmsType::Basen: return "Basen Green / Tianpower";
    default: return "Unknown BMS";
  }
}

inline BmsType bmsTypeFromString(const String& s) {
  if (s == "jk" || s == "jikong") return BmsType::Jk;
  if (s == "basen" || s == "tianpower" || s == "tp") return BmsType::Basen;
  return BmsType::None;
}

inline bool bmsNameLooksLikeHexSerial(const String& name) {
  // Tianpower / Basen often advertise as hex serial only (e.g. 50514652C101132).
  if (name.length() < 10 || name.length() > 24) return false;
  for (unsigned i = 0; i < name.length(); i++) {
    if (!isxdigit((unsigned char)name.charAt(i))) return false;
  }
  return true;
}

inline bool bmsMacLooksEspressif(const String& mac) {
  String m = mac;
  m.toUpperCase();
  return m.startsWith("C8:47:8") || m.startsWith("24:0A:C4") || m.startsWith("30:AE:A4")
      || m.startsWith("A4:CF:12") || m.startsWith("84:CC:A8") || m.startsWith("3C:61:05");
}

inline bool bmsHasServiceUuid(BLEAdvertisedDevice& d, uint16_t uuid16) {
  if (!d.haveServiceUUID()) return false;
  BLEUUID want16(uuid16);
  char buf[40];
  snprintf(buf, sizeof(buf), "0000%04x-0000-1000-8000-00805f9b34fb", uuid16);
  BLEUUID want128(buf);
  for (int i = 0; i < d.getServiceUUIDCount(); i++) {
    BLEUUID u = d.getServiceUUID(i);
    if (u.equals(want16) || u.equals(want128)) return true;
  }
  return d.isAdvertisingService(want16) || d.isAdvertisingService(want128);
}

inline BmsType bmsDetectFromName(const String& name) {
  String n = name;
  n.toUpperCase();
  if (n.indexOf("JK") >= 0) return BmsType::Jk;
  if (n.startsWith("JK_") || n.startsWith("JK-")) return BmsType::Jk;
  if (n.indexOf("-B2A") >= 0 || n.indexOf("_B2A") >= 0) return BmsType::Jk;
  if (n.indexOf("_BD") >= 0 || n.indexOf("-BD") >= 0) return BmsType::Jk;
  if (n.indexOf("PB2A") >= 0 || n.indexOf("PB1A") >= 0) return BmsType::Jk;
  if (n.startsWith("TP_") || n.startsWith("TP-")) return BmsType::Basen;
  if (n.indexOf("BSTBD") >= 0 || n.indexOf("LT55") >= 0 || n.indexOf("LT52") >= 0) return BmsType::Basen;
  if (n.indexOf("TIANPOWER") >= 0 || n.indexOf("TIAN") >= 0) return BmsType::Basen;
  if (n.indexOf("BASEN") >= 0) return BmsType::Basen;
  if (bmsNameLooksLikeHexSerial(n)) return BmsType::Basen;
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
  if (bmsHasServiceUuid(d, 0xff00)) return BmsType::Basen;
  if (bmsHasServiceUuid(d, 0xffe0)) return BmsType::Jk;
  if (bmsHasService(d, "0000ff00-0000-1000-8000-00805f9b34fb")) return BmsType::Basen;
  if (bmsHasService(d, "0000ffe0-0000-1000-8000-00805f9b34fb")) return BmsType::Jk;
  if (bmsMfgMatch(d, 0x4B4A) || bmsMfgMatch(d, 0x0B65)) return BmsType::Jk;

  BmsType byName = bmsDetectFromName(name);
  if (byName != BmsType::None) return byName;

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
  int limit = bms.cellCount > 0 ? bms.cellCount : 16;
  if (limit > 16) limit = 16;
  float minV = 1000, maxV = 0, sum = 0;
  int active = 0, minN = 0, maxN = 0;
  for (int i = 0; i < limit; i++) {
    float v = bms.cellVoltages[i];
    if (v <= 0.5f) continue;
    active++;
    sum += v;
    if (v < minV) { minV = v; minN = i + 1; }
    if (v > maxV) { maxV = v; maxN = i + 1; }
  }
  if (active > 0) {
    if (bms.cellCount <= 0) bms.cellCount = active;
    bms.minCellV = minV;
    bms.maxCellV = maxV;
    bms.deltaCellV = maxV - minV;
    bms.avgCellV = sum / active;
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
  int cellSlots = b.cellCount > 0 ? b.cellCount : 16;
  if (cellSlots > 16) cellSlots = 16;
  for (int i = 0; i < cellSlots; i++) cells.add(b.cellVoltages[i]);

  JsonArray cellBal = o.createNestedArray("cell_balancing");
  for (int i = 0; i < cellSlots; i++) cellBal.add(b.cellBalancing[i]);

  JsonArray temps = o.createNestedArray("temps");
  int tempSlots = b.tempSensorCount > 0 ? b.tempSensorCount : 8;
  if (tempSlots > 8) tempSlots = 8;
  for (int i = 0; i < tempSlots; i++) temps.add(b.temps[i]);
}

inline String bmsToDisplay(const BmsData& b) {
  String tag = bmsTypeLabel(b.type);
  if (!b.connected) return tag + ": not connected";
  if (!b.valid) return tag + ": waiting...";
  String s = tag + "\n";
  if (b.name.length()) s += b.name + "\n";
  s += "SOC " + String(b.soc, 0) + "%  " + String(b.voltage, 2) + "V  " + String(b.current, 2) + "A\n";
  if (b.capacityAh > 0) {
    s += String(b.remainingAh, 1) + "/" + String(b.capacityAh, 1) + " Ah\n";
  }
  return s;
}
