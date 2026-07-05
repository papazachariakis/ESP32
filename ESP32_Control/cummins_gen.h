#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include "modbus_rtu.h"

// PS0600 Modbus map (A029X159 Issue 23). 5-digit form: offset = reg - 40001,
// identical to the doc's 6-digit "4000xx" form (offset = addr - 400001).
#define CUMMINS_REG_CONTROLLER    40009  // Application Device Type
#define CUMMINS_REG_OP_MODE       40010  // Control Switch Position 0=Off 1=Auto 2=Manual
#define CUMMINS_REG_GENSET_STATE  40011  // Genset State (see enum)
#define CUMMINS_REG_ACTIVE_FAULT  40012  // Current Fault number
#define CUMMINS_REG_FAULT_TYPE    40013  // Current Fault Severity 0=None 1=Warn 2=Shutdown
#define CUMMINS_REG_NFPA_FAULT    40016  // NFPA 110 Fault Register (uint32)
#define CUMMINS_REG_VOLT_L1N      40018  // x1 V
#define CUMMINS_REG_VOLT_L2N      40019
#define CUMMINS_REG_VOLT_L3N      40020
#define CUMMINS_REG_VOLT_L1L2     40022
#define CUMMINS_REG_VOLT_L2L3     40023
#define CUMMINS_REG_VOLT_L3L1     40024
#define CUMMINS_REG_CURR_L1       40026  // x1 Amps
#define CUMMINS_REG_CURR_L2       40027
#define CUMMINS_REG_CURR_L3       40028
#define CUMMINS_REG_KW_L1         40031  // int16 x1 kW
#define CUMMINS_REG_KW_L2         40032
#define CUMMINS_REG_KW_L3         40033
#define CUMMINS_REG_KW_TOTAL      40034
#define CUMMINS_REG_KVA_L1        40040  // uint16 x1 kVA
#define CUMMINS_REG_KVA_L2        40041
#define CUMMINS_REG_KVA_L3        40042
#define CUMMINS_REG_KVA_TOTAL     40043
#define CUMMINS_REG_FREQUENCY     40044  // x0.01 Hz
#define CUMMINS_REG_LOAD_L1       40058  // x0.1 % (RMS current %)
#define CUMMINS_REG_LOAD_L2       40059
#define CUMMINS_REG_LOAD_L3       40060
#define CUMMINS_REG_BATTERY_V     40061  // x0.001 V
#define CUMMINS_REG_OIL_PSI       40062  // x0.1 psi
#define CUMMINS_REG_COOLANT_F     40064  // int16 x0.1 degF
#define CUMMINS_REG_ENGINE_RPM    40068  // x0.125 rpm
#define CUMMINS_REG_TOTAL_RUNS    40069  // start attempts
#define CUMMINS_REG_RUNTIME_HI    40070  // uint32 x0.05 hours
#define CUMMINS_REG_RUNTIME_LO    40071
#define CUMMINS_CMD_START         40300  // Modbus Remote Start 0/1
#define CUMMINS_CMD_RESET         40301  // Modbus Fault Reset 0/1
#define CUMMINS_CMD_ESTOP         40302  // Network Shutdown 0/1

#define MODBUS_PROFILE_PS0600 0
#define MODBUS_PROFILE_ENTES  1

#define ENTES_REG_MEAS_START 0
#define ENTES_REG_MEAS_COUNT 26

struct GenData {
  bool valid = false;
  unsigned long lastUpdate = 0;
  uint8_t controllerType = 0;
  uint8_t opMode = 0;
  uint8_t gensetState = 0;
  uint16_t activeFault = 0;
  uint8_t faultType = 0;
  uint32_t nfpaFault = 0;
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
  float loadL1Pct = 0;
  float loadL2Pct = 0;
  float loadL3Pct = 0;
  float batteryV = 0;
  float oilKpa = 0;
  float coolantC = 0;
  uint16_t engineRpm = 0;
  uint16_t totalRuns = 0;
  uint32_t runTimeSec = 0;
  float runtimeHours = 0;
  String lastError;
  String lastScan;
};

// PS0600 abnormal-state sentinels: unsigned 65531..65535, signed 32763..32767.
inline bool cumminsNaU(uint16_t v) { return v >= 0xFFFB; }
inline bool cumminsNaS(int16_t v) { return v >= 32763; }

inline const char* cumminsOpModeLabelEl(uint8_t m) {
  switch (m) {
    case 1: return "Αυτόματο";
    case 2: return "Χειροκίνητο";
    default: return "Off";
  }
}

inline const char* cumminsStateLabelEl(uint8_t s) {
  switch (s) {
    case 0: return "Off";
    case 1: return "Stop";
    case 2: return "Προθέρμανση";
    case 3: return "Προ-εκκίνηση";
    case 4: return "Εκκίνηση (Crank)";
    case 5: return "Αποσύνδεση μίζας";
    case 6: return "Προ-ανέβασμα";
    case 7: return "Ανέβασμα";
    case 8: return "Λειτουργία";
    case 9: return "Σφάλμα Shutdown";
    case 10: return "Prerun Setup";
    case 11: return "Runtime Setup";
    case 12: return "Factory Test";
    case 13: return "Αναμονή Powerdown";
    default: return "Άγνωστη";
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
    case 0: return "Off";
    case 1: return "Stop";
    case 2: return "Preheat";
    case 3: return "Precrank";
    case 4: return "Crank";
    case 5: return "Starter Disconnect";
    case 6: return "PreRamp";
    case 7: return "Ramp";
    case 8: return "Running";
    case 9: return "Fault Shutdown";
    case 10: return "Prerun Setup";
    case 11: return "Runtime Setup";
    case 12: return "Factory Test";
    case 13: return "Waiting For Powerdown";
    default: return "Unknown";
  }
}

inline const char* cumminsFaultTypeLabel(uint8_t t) {
  switch (t) {
    case 1: return "Warning";
    case 2: return "Shutdown";
    default: return "Normal";
  }
}

inline uint32_t entesU32(const uint16_t* r, uint8_t pairIdx) {
  return ((uint32_t)r[pairIdx * 2] << 16) | r[pairIdx * 2 + 1];
}

inline float entesFloat(const uint16_t* r, uint8_t pairIdx) {
  uint32_t u = entesU32(r, pairIdx);
  float f;
  memcpy(&f, &u, sizeof(f));
  return f;
}

inline void genFillJson(JsonObject& o, const GenData& g, uint8_t profile) {
  o["valid"] = g.valid;
  o["profile"] = (profile == MODBUS_PROFILE_ENTES) ? "ENTES_MPR46S" : "PS0600";
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
  o["kw_l1"] = g.kwL1;
  o["kw_l2"] = g.kwL2;
  o["kw_l3"] = g.kwL3;
  o["kw_total"] = g.kwTotal;
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
  o["runtime_hours"] = g.runtimeHours;
  if (g.lastError.length()) o["error"] = g.lastError;
  if (g.lastScan.length()) o["scan_result"] = g.lastScan;
}

struct GenManager {
  HardwareSerial* bus = nullptr;
  GenData data;
  bool enabled = true;
  uint8_t profile = MODBUS_PROFILE_PS0600;
  uint8_t slaveId = 1;
  uint32_t baud = 9600;
  uint16_t probeReg = CUMMINS_REG_CONTROLLER;  // 4xxxx register used by auto-scan
  unsigned long lastPoll = 0;
  uint16_t pollIntervalMs = 3000;
  uint8_t pollStep = 0;

  void load(Preferences& prefs) {
    enabled = prefs.getBool("modbus_en", false);
    profile = (uint8_t)prefs.getUInt("modbus_prof", MODBUS_PROFILE_PS0600);
    slaveId = (uint8_t)prefs.getUInt("modbus_id", 1);
    baud = prefs.getUInt("modbus_baud", 9600);
    probeReg = (uint16_t)prefs.getUInt("modbus_probe", CUMMINS_REG_CONTROLLER);
  }

  void save(Preferences& prefs) {
    prefs.putBool("modbus_en", enabled);
    prefs.putUInt("modbus_prof", profile);
    prefs.putUInt("modbus_id", slaveId);
    prefs.putUInt("modbus_baud", baud);
    prefs.putUInt("modbus_probe", probeReg);
  }

  void begin() {
    if (MODBUS_DE_PIN >= 0) {
      pinMode(MODBUS_DE_PIN, OUTPUT);
      modbusSetTx(MODBUS_DE_PIN, false);
    }
    bus = &Serial2;
    bus->begin(baud, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
    bus->setTimeout(50);
  }

  void applyBaud() {
    if (bus) bus->begin(baud, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
  }

  // Sweep common baud rates x slave IDs looking for a Modbus response on
  // register 40009. Applies and returns the first working combo. Restores the
  // previous settings if nothing responds.
  bool scanBus(uint8_t idMin = 1, uint8_t idMax = 8) {
    static const uint32_t kBauds[] = { 9600, 19200, 38400, 57600, 115200 };
    uint32_t savedBaud = baud;
    uint8_t savedId = slaveId;
    uint16_t addr = modbusHoldAddr(probeReg);
    data.lastScan = "";
    int bestKind = 0;  // 2 = exception (present), 1 = ok (present+readable)
    uint32_t bestBaud = 0;
    uint8_t bestId = 0;
    for (uint8_t bi = 0; bi < 5; bi++) {
      if (bus) bus->begin(kBauds[bi], SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
      delay(80);
      for (uint8_t id = idMin; id <= idMax; id++) {
        int kind = modbusProbeRegister(*bus, MODBUS_DE_PIN, id, addr, 350);
        if (kind == 1) {
          baud = kBauds[bi];
          slaveId = id;
          data.lastScan = "FOUND baud=" + String(kBauds[bi]) + " slave=" + String(id) + " reg=" + String(probeReg);
          return true;
        }
        if (kind == 2 && bestKind == 0) {
          bestKind = 2;
          bestBaud = kBauds[bi];
          bestId = id;
        }
      }
    }
    if (bestKind == 2) {
      baud = bestBaud;
      slaveId = bestId;
      data.lastScan = "DEVICE at baud=" + String(bestBaud) + " slave=" + String(bestId) + " but reg " + String(probeReg) + " rejected - try another Probe register";
      return true;
    }
    baud = savedBaud;
    slaveId = savedId;
    applyBaud();
    data.lastScan = "NONE - no response 9600-115200, id " + String(idMin) + "-" + String(idMax) + " (check A/B swap & PS0600 Modbus enabled)";
    return false;
  }

  bool readBlock(uint16_t reg40001, uint16_t count, uint16_t* out) {
    return modbusReadHolding(*bus, MODBUS_DE_PIN, slaveId, modbusHoldAddr(reg40001), count, out);
  }

  bool readDirect(uint16_t startReg, uint16_t count, uint16_t* out) {
    return modbusReadHolding(*bus, MODBUS_DE_PIN, slaveId, startReg, count, out, 2000);
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
    if (!enabled || !bus || !action || profile != MODBUS_PROFILE_PS0600) return false;
    if (strcmp(action, "start") == 0) return cmdStart();
    if (strcmp(action, "stop") == 0) return cmdStop();
    if (strcmp(action, "reset") == 0) return cmdFaultReset();
    if (strcmp(action, "estop_on") == 0) return cmdEstop(true);
    if (strcmp(action, "estop_off") == 0) return cmdEstop(false);
    return false;
  }

  void pollReset() { pollStep = 0; }

  bool pollEntesOnce() {
    uint16_t r[ENTES_REG_MEAS_COUNT];
    if (!readDirect(ENTES_REG_MEAS_START, ENTES_REG_MEAS_COUNT, r)) {
      data.lastError = "modbus 0 - ENTES/RS485 RX2 TX2";
      return false;
    }
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
    uint16_t p[10];
    if (readDirect(26, 10, p)) {
      data.kvaL1 = entesFloat(p, 0) / 1000.0f;
      data.kvaL2 = entesFloat(p, 1) / 1000.0f;
      data.kvaL3 = entesFloat(p, 2) / 1000.0f;
      data.kvaTotal = entesFloat(p, 4) / 1000.0f;
    }
    data.valid = true;
    data.lastUpdate = millis();
    data.lastError = "";
    return true;
  }

  bool pollStepOnce() {
    if (!enabled || !bus) return false;
    if (profile == MODBUS_PROFILE_ENTES) return pollEntesOnce();

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
        data.nfpaFault = ((uint32_t)f[0] << 16) | f[1];
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
        data.voltL1N = cumminsNaU(v[0]) ? 0 : (float)v[0];
        data.voltL2N = cumminsNaU(v[1]) ? 0 : (float)v[1];
        data.voltL3N = cumminsNaU(v[2]) ? 0 : (float)v[2];
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
        data.voltL1L2 = cumminsNaU(ll[0]) ? 0 : (float)ll[0];
        data.voltL2L3 = cumminsNaU(ll[1]) ? 0 : (float)ll[1];
        data.voltL3L1 = cumminsNaU(ll[2]) ? 0 : (float)ll[2];
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
        data.currL1 = cumminsNaU(c[0]) ? 0 : (float)c[0];
        data.currL2 = cumminsNaU(c[1]) ? 0 : (float)c[1];
        data.currL3 = cumminsNaU(c[2]) ? 0 : (float)c[2];
        data.currAvg = (data.currL1 + data.currL2 + data.currL3) / 3.0f;
        data.voltAvgLL = (data.voltL1L2 + data.voltL2L3 + data.voltL3L1) / 3.0f;
        pollStep = 5;
        return false;
      }
      case 5: {
        uint16_t k[4];
        if (!readBlock(CUMMINS_REG_KW_L1, 4, k)) {
          data.lastError = "modbus 40031";
          pollStep = 0;
          return false;
        }
        data.kwL1 = cumminsNaS((int16_t)k[0]) ? 0 : (int16_t)k[0];
        data.kwL2 = cumminsNaS((int16_t)k[1]) ? 0 : (int16_t)k[1];
        data.kwL3 = cumminsNaS((int16_t)k[2]) ? 0 : (int16_t)k[2];
        data.kwTotal = cumminsNaS((int16_t)k[3]) ? 0 : (int16_t)k[3];
        pollStep = 6;
        return false;
      }
      case 6: {
        uint16_t p[5];
        if (!readBlock(CUMMINS_REG_KVA_L1, 5, p)) {
          data.lastError = "modbus 40040";
          pollStep = 0;
          return false;
        }
        data.kvaL1 = cumminsNaU(p[0]) ? 0 : (float)p[0];
        data.kvaL2 = cumminsNaU(p[1]) ? 0 : (float)p[1];
        data.kvaL3 = cumminsNaU(p[2]) ? 0 : (float)p[2];
        data.kvaTotal = cumminsNaU(p[3]) ? 0 : (float)p[3];
        data.frequency = cumminsNaU(p[4]) ? 0 : p[4] * 0.01f;
        pollStep = 7;
        return false;
      }
      case 7: {
        uint16_t l[3];
        if (!readBlock(CUMMINS_REG_LOAD_L1, 3, l)) {
          data.lastError = "modbus 40058";
          pollStep = 0;
          return false;
        }
        data.loadL1Pct = cumminsNaU(l[0]) ? 0 : l[0] * 0.1f;
        data.loadL2Pct = cumminsNaU(l[1]) ? 0 : l[1] * 0.1f;
        data.loadL3Pct = cumminsNaU(l[2]) ? 0 : l[2] * 0.1f;
        pollStep = 8;
        return false;
      }
      case 8: {
        uint16_t e[2];
        if (!readBlock(CUMMINS_REG_BATTERY_V, 2, e)) {
          data.lastError = "modbus 40061";
          pollStep = 0;
          return false;
        }
        data.batteryV = cumminsNaU(e[0]) ? 0 : e[0] * 0.001f;
        // Oil pressure register is psi x0.1; convert to kPa for display.
        data.oilKpa = cumminsNaU(e[1]) ? 0 : (e[1] * 0.1f) * 6.894757f;
        pollStep = 9;
        return false;
      }
      case 9: {
        uint16_t t[1];
        if (!readBlock(CUMMINS_REG_COOLANT_F, 1, t)) {
          data.lastError = "modbus 40064";
          pollStep = 0;
          return false;
        }
        // Coolant register is degF x0.1 (int16); convert to degC.
        int16_t rawF = (int16_t)t[0];
        data.coolantC = cumminsNaS(rawF) ? 0 : ((rawF * 0.1f) - 32.0f) * (5.0f / 9.0f);
        pollStep = 10;
        return false;
      }
      case 10: {
        uint16_t r[4];
        if (!readBlock(CUMMINS_REG_ENGINE_RPM, 4, r)) {
          data.lastError = "modbus 40068";
          pollStep = 0;
          return false;
        }
        data.engineRpm = cumminsNaU(r[0]) ? 0 : (uint16_t)(r[0] * 0.125f);
        data.totalRuns = cumminsNaU(r[1]) ? 0 : r[1];
        // Engine Running Time: uint32 x0.05 hours.
        uint32_t rtRaw = ((uint32_t)r[2] << 16) | r[3];
        data.runtimeHours = rtRaw * 0.05f;
        data.runTimeSec = (uint32_t)(data.runtimeHours * 3600.0f);
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
    if (profile == MODBUS_PROFILE_ENTES) return pollEntesOnce();
    for (uint8_t i = 0; i < 20; i++) {
      if (pollStepOnce()) return true;
      if (pollStep == 0 && i < 19) return false;
    }
    return data.valid;
  }

  void poll() {
    if (!enabled) return;
    if (profile == MODBUS_PROFILE_ENTES) {
      if (millis() - lastPoll < pollIntervalMs) return;
      lastPoll = millis();
      pollEntesOnce();
      return;
    }
    if (pollStep == 0 && millis() - lastPoll < pollIntervalMs) return;
    if (pollStep == 0) {
      data.valid = false;
      lastPoll = millis();
    }
    pollStepOnce();
  }
};
