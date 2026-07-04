#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include "modbus_rtu.h"

#define CUMMINS_REG_CONTROLLER    40009
#define CUMMINS_REG_OP_MODE       40010
#define CUMMINS_REG_GENSET_STATE  40011
#define CUMMINS_REG_ACTIVE_FAULT  40012
#define CUMMINS_REG_FAULT_TYPE    40013
#define CUMMINS_REG_NFPA_FAULT    40016
#define CUMMINS_REG_VOLT_L1N      40018
#define CUMMINS_REG_VOLT_L2N      40019
#define CUMMINS_REG_VOLT_L3N      40020
#define CUMMINS_REG_VOLT_L1L2     40022
#define CUMMINS_REG_VOLT_L2L3     40023
#define CUMMINS_REG_VOLT_L3L1     40024
#define CUMMINS_REG_CURR_L1       40026
#define CUMMINS_REG_CURR_L2       40027
#define CUMMINS_REG_CURR_L3       40028
#define CUMMINS_REG_KVA_L1        40040
#define CUMMINS_REG_KVA_L2        40041
#define CUMMINS_REG_KVA_L3        40042
#define CUMMINS_REG_KVA_TOTAL     40043
#define CUMMINS_REG_FREQUENCY     40044
#define CUMMINS_REG_LOAD_L1       40058
#define CUMMINS_REG_LOAD_L2       40059
#define CUMMINS_REG_LOAD_L3       40060
#define CUMMINS_REG_BATTERY_V     40061
#define CUMMINS_REG_OIL_KPA       40062
#define CUMMINS_REG_COOLANT_C     40064
#define CUMMINS_REG_ENGINE_RPM    40068
#define CUMMINS_REG_TOTAL_RUNS    40069
#define CUMMINS_REG_RUNTIME_HI    40070
#define CUMMINS_REG_RUNTIME_LO    40071
#define CUMMINS_CMD_START         400300
#define CUMMINS_CMD_RESET         400301
#define CUMMINS_CMD_ESTOP         400302

struct GenData {
  bool valid = false;
  unsigned long lastUpdate = 0;
  uint8_t controllerType = 0;
  uint8_t opMode = 0;
  uint8_t gensetState = 0;
  uint16_t activeFault = 0;
  uint8_t faultType = 0;
  uint16_t nfpaFault = 0;
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
  float kvaL1 = 0;
  float kvaL2 = 0;
  float kvaL3 = 0;
  float kvaTotal = 0;
  float loadL1Pct = 0;
  float loadL2Pct = 0;
  float loadL3Pct = 0;
  float batteryV = 0;
  float oilKpa = 0;
  float coolantC = 0;
  uint16_t engineRpm = 0;
  uint16_t totalRuns = 0;
  uint32_t runTimeSec = 0;
  String lastError;
};

inline const char* cumminsOpModeLabelEl(uint8_t m) {
  switch (m) {
    case 1: return "Αυτόματο";
    case 2: return "Χειροκίνητο";
    default: return "Off";
  }
}

inline const char* cumminsStateLabelEl(uint8_t s) {
  switch (s) {
    case 8: return "Λειτουργία";
    case 9: return "Σφάλμα Shutdown";
    case 4: return "Εκκίνηση";
    case 7: return "Ανέβασμα";
    case 1: return "Stop";
    default: return "Έτοιμη";
  }
}

inline const char* cumminsOpModeLabel(uint8_t m) {
  switch (m) {
    case 1: return "Auto";
    case 2: return "Manual";
    default: return "Off";
  }
}

inline const char* cumminsStateLabel(uint8_t s) {
  switch (s) {
    case 8: return "Running";
    case 9: return "Fault Shutdown";
    case 4: return "Crank";
    case 7: return "Ramp";
    default: return "Ready";
  }
}

inline const char* cumminsFaultTypeLabel(uint8_t t) {
  switch (t) {
    case 1: return "Warning";
    case 2: return "Shutdown";
    default: return "Normal";
  }
}

inline void genFillJson(JsonObject& o, const GenData& g) {
  o["valid"] = g.valid;
  o["profile"] = "PS0600";
  o["controller_type"] = g.controllerType;
  o["op_mode"] = g.opMode;
  o["op_mode_label"] = cumminsOpModeLabel(g.opMode);
  o["op_mode_label_el"] = cumminsOpModeLabelEl(g.opMode);
  o["genset_state"] = g.gensetState;
  o["genset_state_label"] = cumminsStateLabel(g.gensetState);
  o["genset_state_label_el"] = cumminsStateLabelEl(g.gensetState);
  o["running"] = (g.gensetState == 8);
  o["active_fault"] = g.activeFault;
  o["fault_type"] = g.faultType;
  o["fault_type_label"] = cumminsFaultTypeLabel(g.faultType);
  o["nfpa_fault"] = g.nfpaFault;
  o["volt_l1n"] = g.voltL1N;
  o["volt_l2n"] = g.voltL2N;
  o["volt_l3n"] = g.voltL3N;
  o["volt_l1l2"] = g.voltL1L2;
  o["volt_l2l3"] = g.voltL2L3;
  o["volt_l3l1"] = g.voltL3L1;
  o["volt_avg_ll"] = g.voltAvgLL;
  o["curr_l1"] = g.currL1;
  o["curr_l2"] = g.currL2;
  o["curr_l3"] = g.currL3;
  o["curr_avg"] = g.currAvg;
  o["frequency"] = g.frequency;
  o["kva_l1"] = g.kvaL1;
  o["kva_l2"] = g.kvaL2;
  o["kva_l3"] = g.kvaL3;
  o["kva_total"] = g.kvaTotal;
  o["load_l1_pct"] = g.loadL1Pct;
  o["load_l2_pct"] = g.loadL2Pct;
  o["load_l3_pct"] = g.loadL3Pct;
  o["battery_v"] = g.batteryV;
  o["oil_kpa"] = g.oilKpa;
  o["coolant_c"] = g.coolantC;
  o["engine_rpm"] = g.engineRpm;
  o["total_runs"] = g.totalRuns;
  o["runtime_sec"] = g.runTimeSec;
  if (g.lastError.length()) o["error"] = g.lastError;
}

struct GenManager {
  HardwareSerial* bus = nullptr;
  GenData data;
  bool enabled = true;
  uint8_t slaveId = 1;
  uint32_t baud = 9600;
  unsigned long lastPoll = 0;
  uint16_t pollIntervalMs = 3000;
  uint8_t pollStep = 0;

  void load(Preferences& prefs) {
    enabled = prefs.getBool("modbus_en", false);
    slaveId = (uint8_t)prefs.getUInt("modbus_id", 1);
    baud = prefs.getUInt("modbus_baud", 9600);
  }

  void save(Preferences& prefs) {
    prefs.putBool("modbus_en", enabled);
    prefs.putUInt("modbus_id", slaveId);
    prefs.putUInt("modbus_baud", baud);
  }

  void begin() {
    pinMode(MODBUS_DE_PIN, OUTPUT);
    modbusSetTx(MODBUS_DE_PIN, false);
    bus = &Serial2;
    bus->begin(baud, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
    bus->setTimeout(50);
  }

  void applyBaud() {
    if (bus) bus->begin(baud, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
  }

  bool readBlock(uint16_t reg40001, uint16_t count, uint16_t* out) {
    return modbusReadHolding(*bus, MODBUS_DE_PIN, slaveId, modbusHoldAddr(reg40001), count, out);
  }

  bool writeHold(uint16_t reg40001, uint16_t value) {
    if (!bus) return false;
    return modbusWriteSingle(*bus, MODBUS_DE_PIN, slaveId, modbusHoldAddr(reg40001), value);
  }

  bool cmdStart() { return writeHold(CUMMINS_CMD_START, 1); }
  bool cmdStop() { return writeHold(CUMMINS_CMD_START, 0); }
  bool cmdFaultReset() { return writeHold(CUMMINS_CMD_RESET, 1); }
  bool cmdEstop(bool active) { return writeHold(CUMMINS_CMD_ESTOP, active ? 1 : 0); }

  bool runGensetCmd(const char* action) {
    if (!enabled || !bus || !action) return false;
    if (strcmp(action, "start") == 0) return cmdStart();
    if (strcmp(action, "stop") == 0) return cmdStop();
    if (strcmp(action, "reset") == 0) return cmdFaultReset();
    if (strcmp(action, "estop_on") == 0) return cmdEstop(true);
    if (strcmp(action, "estop_off") == 0) return cmdEstop(false);
    return false;
  }

  void pollReset() { pollStep = 0; }

  bool pollStepOnce() {
    if (!enabled || !bus) return false;

    switch (pollStep) {
      case 0: {
        uint16_t a[5];
        if (!readBlock(CUMMINS_REG_CONTROLLER, 5, a)) {
          data.lastError = "modbus 40009 - check RS485 wiring";
          pollStep = 0;
          return false;
        }
        data.controllerType = (uint8_t)a[0];
        data.opMode = (uint8_t)a[1];
        data.gensetState = (uint8_t)a[2];
        data.activeFault = a[3];
        data.faultType = (uint8_t)a[4];
        pollStep = 1;
        return false;
      }
      case 1: {
        uint16_t f[2];
        if (!readBlock(CUMMINS_REG_NFPA_FAULT, 2, f)) {
          data.lastError = "modbus 40016";
          pollStep = 0;
          return false;
        }
        data.nfpaFault = f[1];
        pollStep = 2;
        return false;
      }
      case 2: {
        uint16_t v[3];
        if (!readBlock(CUMMINS_REG_VOLT_L1N, 3, v)) {
          data.lastError = "modbus 40018";
          pollStep = 0;
          return false;
        }
        data.voltL1N = v[0];
        data.voltL2N = v[1];
        data.voltL3N = v[2];
        pollStep = 3;
        return false;
      }
      case 3: {
        uint16_t ll[3];
        if (!readBlock(CUMMINS_REG_VOLT_L1L2, 3, ll)) {
          data.lastError = "modbus 40022";
          pollStep = 0;
          return false;
        }
        data.voltL1L2 = ll[0];
        data.voltL2L3 = ll[1];
        data.voltL3L1 = ll[2];
        pollStep = 4;
        return false;
      }
      case 4: {
        uint16_t c[3];
        if (!readBlock(CUMMINS_REG_CURR_L1, 3, c)) {
          data.lastError = "modbus 40026";
          pollStep = 0;
          return false;
        }
        data.currL1 = c[0] * 0.1f;
        data.currL2 = c[1] * 0.1f;
        data.currL3 = c[2] * 0.1f;
        data.currAvg = (data.currL1 + data.currL2 + data.currL3) / 3.0f;
        data.voltAvgLL = (data.voltL1L2 + data.voltL2L3 + data.voltL3L1) / 3.0f;
        pollStep = 5;
        return false;
      }
      case 5: {
        uint16_t p[5];
        if (!readBlock(CUMMINS_REG_KVA_L1, 5, p)) {
          data.lastError = "modbus 40040";
          pollStep = 0;
          return false;
        }
        data.kvaL1 = p[0];
        data.kvaL2 = p[1];
        data.kvaL3 = p[2];
        data.kvaTotal = p[3];
        data.frequency = p[4] * 0.1f;
        pollStep = 6;
        return false;
      }
      case 6: {
        uint16_t l[3];
        if (!readBlock(CUMMINS_REG_LOAD_L1, 3, l)) {
          data.lastError = "modbus 40058";
          pollStep = 0;
          return false;
        }
        data.loadL1Pct = l[0] * 0.1f;
        data.loadL2Pct = l[1] * 0.1f;
        data.loadL3Pct = l[2] * 0.1f;
        pollStep = 7;
        return false;
      }
      case 7: {
        uint16_t e[2];
        if (!readBlock(CUMMINS_REG_BATTERY_V, 2, e)) {
          data.lastError = "modbus 40061";
          pollStep = 0;
          return false;
        }
        data.batteryV = e[0] * 0.1f;
        data.oilKpa = e[1] * 0.1f;
        pollStep = 8;
        return false;
      }
      case 8: {
        uint16_t t[1];
        if (!readBlock(CUMMINS_REG_COOLANT_C, 1, t)) {
          data.lastError = "modbus 40064";
          pollStep = 0;
          return false;
        }
        data.coolantC = t[0] * 0.1f;
        pollStep = 9;
        return false;
      }
      case 9: {
        uint16_t r[4];
        if (!readBlock(CUMMINS_REG_ENGINE_RPM, 4, r)) {
          data.lastError = "modbus 40068";
          pollStep = 0;
          return false;
        }
        data.engineRpm = r[0];
        data.totalRuns = r[1];
        data.runTimeSec = ((uint32_t)r[2] << 16) | r[3];
        data.valid = true;
        data.lastUpdate = millis();
        data.lastError = "";
        pollStep = 0;
        return true;
      }
      default:
        pollStep = 0;
        return false;
    }
  }

  bool pollOnce() {
    pollReset();
    data.valid = false;
    for (uint8_t i = 0; i < 20; i++) {
      if (pollStepOnce()) return true;
      if (pollStep == 0 && i < 19) return false;
    }
    return data.valid;
  }

  void poll() {
    if (!enabled) return;
    if (pollStep == 0 && millis() - lastPoll < pollIntervalMs) return;
    if (pollStep == 0) {
      data.valid = false;
      lastPoll = millis();
    }
    pollStepOnce();
  }
};
