#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <HardwareSerial.h>
#include "modbus_rtu.h"

#ifndef MODBUS_DEFAULT_METER_ENABLED
#define MODBUS_DEFAULT_METER_ENABLED true
#endif
#ifndef MODBUS_DEFAULT_METER_SLAVE_ID
#define MODBUS_DEFAULT_METER_SLAVE_ID 2
#endif

#define ENTES_MEAS_REG_COUNT 26
#define ENTES_PWR_REG_START 26
#define ENTES_PWR_REG_COUNT 18
#define ENTES_ENERGY_WH_REG 216
#define ENTES_ENERGY_GEN_REG 534

inline uint32_t entesU32(const uint16_t* r, uint8_t pairIdx) {
  return ((uint32_t)r[pairIdx * 2] << 16) | r[pairIdx * 2 + 1];
}

inline float entesFloat(const uint16_t* r, uint8_t pairIdx) {
  uint32_t u = entesU32(r, pairIdx);
  float f;
  memcpy(&f, &u, sizeof(f));
  return f;
}

inline uint64_t entesU64(const uint16_t* r) {
  return ((uint64_t)r[0] << 48) | ((uint64_t)r[1] << 32) | ((uint64_t)r[2] << 16) | r[3];
}

struct MeterData {
  bool valid = false;
  bool pollComplete = false;
  unsigned long lastUpdate = 0;
  float voltL1N = 0;
  float voltL2N = 0;
  float voltL3N = 0;
  float voltL1L2 = 0;
  float voltL2L3 = 0;
  float voltL3L1 = 0;
  float voltAvgLL = 0;
  float currL1 = 0;
  float currL2 = 0;
  float currL3 = 0;
  float currAvg = 0;
  float frequency = 0;
  float kwL1 = 0;
  float kwL2 = 0;
  float kwL3 = 0;
  float kwTotal = 0;
  float kvaL1 = 0;
  float kvaL2 = 0;
  float kvaL3 = 0;
  float kvaTotal = 0;
  double kwhConsumed = 0;
  double kwhGenerator = 0;
  String lastError;
};

inline void meterFillJson(JsonObject& o, const MeterData& m) {
  o["valid"] = m.valid;
  o["poll_complete"] = m.pollComplete;
  o["profile"] = "ENTES_MPR46S";
  o["device"] = "grid_meter";
  o["volt_l1n"] = m.voltL1N;
  o["volt_l2n"] = m.voltL2N;
  o["volt_l3n"] = m.voltL3N;
  o["volt_l1l2"] = m.voltL1L2;
  o["volt_l2l3"] = m.voltL2L3;
  o["volt_l3l1"] = m.voltL3L1;
  o["volt_avg_ll"] = m.voltAvgLL;
  o["curr_l1"] = m.currL1;
  o["curr_l2"] = m.currL2;
  o["curr_l3"] = m.currL3;
  o["curr_avg"] = m.currAvg;
  o["frequency"] = m.frequency;
  o["kw_l1"] = m.kwL1;
  o["kw_l2"] = m.kwL2;
  o["kw_l3"] = m.kwL3;
  o["kw_total"] = m.kwTotal;
  o["kva_l1"] = m.kvaL1;
  o["kva_l2"] = m.kvaL2;
  o["kva_l3"] = m.kvaL3;
  o["kva_total"] = m.kvaTotal;
  o["kwh_consumed"] = m.kwhConsumed;
  o["kwh_generator"] = m.kwhGenerator;
  if (m.lastError.length()) o["error"] = m.lastError;
  if (m.lastUpdate) {
    o["last_update_ms"] = m.lastUpdate;
    o["age_sec"] = (millis() - m.lastUpdate) / 1000;
  }
}

struct MeterManager {
  HardwareSerial* bus = nullptr;
  int dePin = -1;
  MeterData data;
  bool enabled = true;
  uint8_t slaveId = MODBUS_DEFAULT_METER_SLAVE_ID;
  unsigned long lastPoll = 0;
  uint16_t pollIntervalMs = 4500;
  uint8_t pollStep = 0;
  uint8_t failStreak = 0;
  bool publishPending = false;

  void load(Preferences& prefs) {
    enabled = prefs.getBool("meter_en", MODBUS_DEFAULT_METER_ENABLED);
    slaveId = (uint8_t)prefs.getUInt("meter_id", MODBUS_DEFAULT_METER_SLAVE_ID);
  }

  void save(Preferences& prefs) {
    prefs.putBool("meter_en", enabled);
    prefs.putUInt("meter_id", slaveId);
  }

  void attach(HardwareSerial* serial, int de) {
    bus = serial;
    dePin = de;
  }

  bool takePublishPending() {
    if (!publishPending) return false;
    publishPending = false;
    return true;
  }

  bool readAt(uint16_t startReg, uint16_t count, uint16_t* out) {
    if (!bus) return false;
    return modbusReadHolding(*bus, dePin, slaveId, startReg, count, out, 2000);
  }

  void parseMeasurements(const uint16_t* r) {
    data.voltL1N = entesU32(r, 0) * 0.1f;
    data.voltL2N = entesU32(r, 1) * 0.1f;
    data.voltL3N = entesU32(r, 2) * 0.1f;
    data.voltL1L2 = entesU32(r, 4) * 0.1f;
    data.voltL2L3 = entesU32(r, 5) * 0.1f;
    data.voltL3L1 = entesU32(r, 6) * 0.1f;
    data.currL1 = entesU32(r, 7) * 0.001f;
    data.currL2 = entesU32(r, 8) * 0.001f;
    data.currL3 = entesU32(r, 9) * 0.001f;
    data.currAvg = (data.currL1 + data.currL2 + data.currL3) / 3.0f;
    data.voltAvgLL = (data.voltL1L2 + data.voltL2L3 + data.voltL3L1) / 3.0f;
    data.frequency = entesU32(r, 12) * 0.01f;
  }

  void parsePower(const uint16_t* p) {
    data.kwL1 = entesFloat(p, 0) / 1000.0f;
    data.kwL2 = entesFloat(p, 1) / 1000.0f;
    data.kwL3 = entesFloat(p, 2) / 1000.0f;
    data.kwTotal = entesFloat(p, 4) / 1000.0f;
    if (data.kwTotal <= 0.0f)
      data.kwTotal = data.kwL1 + data.kwL2 + data.kwL3;
    data.kvaL1 = entesFloat(p, 8) / 1000.0f;
    data.kvaL2 = entesFloat(p, 9) / 1000.0f;
    data.kvaL3 = entesFloat(p, 10) / 1000.0f;
    data.kvaTotal = entesFloat(p, 12) / 1000.0f;
    if (data.kvaTotal <= 0.0f)
      data.kvaTotal = data.kvaL1 + data.kvaL2 + data.kvaL3;
  }

  bool pollStepOnce() {
    switch (pollStep) {
      case 0: {
        uint16_t r[ENTES_MEAS_REG_COUNT];
        if (!readAt(0, ENTES_MEAS_REG_COUNT, r)) {
          data.lastError = "meter reg 0 - check slave " + String(slaveId);
          if (++failStreak >= 4) {
            data.valid = false;
            data.pollComplete = false;
          }
          pollStep = 0;
          return false;
        }
        failStreak = 0;
        parseMeasurements(r);
        pollStep = 1;
        modbusBusGap();
        return false;
      }
      case 1: {
        uint16_t p[ENTES_PWR_REG_COUNT];
        if (readAt(ENTES_PWR_REG_START, ENTES_PWR_REG_COUNT, p))
          parsePower(p);
        pollStep = 2;
        modbusBusGap();
        return false;
      }
      case 2: {
        uint16_t e[4];
        if (readAt(ENTES_ENERGY_WH_REG, 4, e))
          data.kwhConsumed = entesU64(e) / 1000.0;
        uint16_t g[4];
        if (readAt(ENTES_ENERGY_GEN_REG, 4, g))
          data.kwhGenerator = (double)entesU64(g);
        data.valid = true;
        data.pollComplete = true;
        data.lastUpdate = millis();
        data.lastError = "";
        publishPending = true;
        pollStep = 0;
        return true;
      }
      default:
        pollStep = 0;
        return false;
    }
  }

  void poll() {
    if (!enabled || !bus) return;
    if (pollStep == 0 && lastPoll != 0 && millis() - lastPoll < pollIntervalMs) return;
    if (pollStep == 0) lastPoll = millis();

    uint32_t budgetStart = millis();
    do {
      if (pollStepOnce()) break;
      modbusPump();
    } while (pollStep != 0 && millis() - budgetStart < 45);

    if (data.valid && data.lastUpdate && millis() - data.lastUpdate > 120000) {
      data.valid = false;
      data.pollComplete = false;
      data.lastError = "meter timeout";
    }
  }
};
