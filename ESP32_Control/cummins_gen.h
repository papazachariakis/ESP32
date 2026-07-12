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
#define CUMMINS_REG_DELAY_START   402355  // TDES, x0.1 sec
#define CUMMINS_REG_DELAY_STOP    402356  // TDEC (cooldown), x0.1 sec
#define CUMMINS_REG_TIME_DELAY_START 403006  // pre-crank delay, x1 sec
#define CUMMINS_REG_TIME_DELAY_STOP  403007  // pre-cooldown delay, x1 sec
#define CUMMINS_CMD_START         40300  // Modbus Remote Start 0/1
#define CUMMINS_CMD_RESET         40301  // Modbus Fault Reset 0/1
#define CUMMINS_CMD_ESTOP         40302  // Network Shutdown 0/1

#define MODBUS_PROFILE_PS0600 0
#define MODBUS_PROFILE_ENTES  1

#define ENTES_REG_MEAS_START 0
#define ENTES_REG_MEAS_COUNT 26

struct GenData {
  bool valid = false;
  bool pollComplete = false;
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
  float delayStartSec = 0;
  float delayStopSec = 0;
  float delayStartPreSec = 0;
  float delayStopPreSec = 0;
  float delayStartRemainSec = 0;
  float delayStopRemainSec = 0;
  bool delayStartActive = false;
  bool delayStopActive = false;
  String lastError;
  String lastScan;
  String lastCmd;
  String lastCmdDetail;
  bool lastCmdOk = false;
  uint16_t remoteStartReg = 0;
  uint16_t networkShutdownReg = 0;
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
  o["poll_complete"] = g.pollComplete;
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
  o["delay_start_sec"] = g.delayStartPreSec;
  o["delay_stop_sec"] = g.delayStopPreSec;
  o["delay_start_pre_sec"] = g.delayStartPreSec;
  o["delay_stop_pre_sec"] = g.delayStopPreSec;
  o["tdes_sec"] = g.delayStartSec;
  o["tdec_sec"] = g.delayStopSec;
  o["delay_start_remain_sec"] = g.delayStartRemainSec;
  o["delay_stop_remain_sec"] = g.delayStopRemainSec;
  o["delay_start_active"] = g.delayStartActive;
  o["delay_stop_active"] = g.delayStopActive;
  if (g.lastError.length()) o["error"] = g.lastError;
  if (g.lastScan.length()) o["scan_result"] = g.lastScan;
  if (g.lastCmd.length()) {
    o["last_cmd"] = g.lastCmd;
    o["last_cmd_ok"] = g.lastCmdOk;
    if (g.lastCmdDetail.length()) o["last_cmd_detail"] = g.lastCmdDetail;
  }
  o["remote_start_reg"] = g.remoteStartReg;
  o["network_shutdown_reg"] = g.networkShutdownReg;
  if (g.lastUpdate) {
    o["last_update_ms"] = g.lastUpdate;
    o["age_sec"] = (millis() - g.lastUpdate) / 1000;
  }
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
  uint8_t pollFailStreak = 0;
  uint8_t pollStep = 0;
  bool pollPaused = false;
  bool publishPending = false;
  bool startDelayArmed = false;
  bool stopDelayArmed = false;
  unsigned long startDelayT0 = 0;
  unsigned long stopDelayT0 = 0;
  uint16_t lastRemoteStart = 0xffff;
  unsigned long lastDelayPublish = 0;
  static const uint8_t kPollBudgetMs = 35;

  void markUpdated(bool publish = false) {
    data.lastUpdate = millis();
    data.valid = true;
    if (publish) publishPending = true;
  }

  bool takePublishPending() {
    if (!publishPending) return false;
    publishPending = false;
    return true;
  }

  void flushBus() {
    if (bus) while (bus->available()) bus->read();
  }

  bool readBlockGap(uint32_t reg40001, uint16_t count, uint16_t* out) {
    if (!readBlock(reg40001, count, out)) return false;
    modbusBusGap();
    return true;
  }

  bool writeHold(uint32_t reg40001, uint16_t value, uint8_t* exceptionOut = nullptr) {
    if (!bus) return false;
    return modbusWriteSingle(*bus, MODBUS_DE_PIN, slaveId, modbusHoldAddr(reg40001), value,
                             MODBUS_WRITE_TIMEOUT_MS, exceptionOut);
  }

  bool readCmdRegs() {
    uint16_t c[3];
    if (!readBlock(CUMMINS_CMD_START, 3, c)) return false;
    data.remoteStartReg = c[0];
    data.networkShutdownReg = c[2];
    return true;
  }

  String writeError(uint32_t reg40001, uint8_t exc) const {
    if (exc == 0) return "write " + String(reg40001) + " no response";
    return "write " + String(reg40001) + " modbus exc " + String(exc);
  }

  bool writeHoldRetry(uint32_t reg40001, uint16_t value, uint8_t tries = 3) {
    uint8_t exc = 0;
    for (uint8_t i = 0; i < tries; i++) {
      if (writeHold(reg40001, value, &exc)) return true;
      modbusBusGap(50);
    }
    data.lastError = writeError(reg40001, exc);
    return false;
  }

  bool cmdStart() {
    uint8_t exc = 0;
    // Clear network shutdown / E-stop Modbus latch if active.
    if (readCmdRegs() && data.networkShutdownReg) {
      writeHold(CUMMINS_CMD_ESTOP, 0, &exc);
      modbusBusGap(80);
    }
    if (data.activeFault > 0) cmdFaultReset();

    if (!writeHold(CUMMINS_CMD_START, 0, &exc)) {
      data.lastError = writeError(CUMMINS_CMD_START, exc);
      return false;
    }
    modbusBusGap(100);

    if (!writeHold(CUMMINS_CMD_START, 1, &exc)) {
      data.lastError = writeError(CUMMINS_CMD_START, exc);
      return false;
    }
    modbusBusGap(400);

    // PS0600 keeps remote start Active while cranking — verify readback when readable.
    if (readCmdRegs()) {
      if (data.remoteStartReg != 1) {
        data.lastError = "40300 readback=" + String(data.remoteStartReg) + " (expected 1)";
        return false;
      }
    }
    return true;
  }

  bool cmdStop() {
    refreshStatusBlock();
    readCmdRegs();
    uint8_t exc = 0;

    if (!writeHoldRetry(CUMMINS_CMD_START, 0)) return false;

    for (uint8_t i = 0; i < 8; i++) {
      modbusBusGap(250);
      readCmdRegs();
      refreshStatusBlock();
      if (data.remoteStartReg == 0) break;
      if (data.gensetState == 13 || data.gensetState <= 1) break;
      if (i == 3 || i == 6) writeHold(CUMMINS_CMD_START, 0, &exc);
    }

    const bool running = data.gensetState == 8 || data.engineRpm > 100;
    const bool latched = data.remoteStartReg == 0 || data.gensetState == 13 || data.gensetState <= 1;

    if (!latched && running) {
      data.lastError = "stop απορρίφθηκε: 40300=" + String(data.remoteStartReg)
        + " state=" + String(data.gensetState) + " rpm=" + String(data.engineRpm);
      return false;
    }

    if (running) {
      const float total = delayStopTotalSec();
      data.lastError = total > 0
        ? "stop OK • απομένουν ~" + String(total, 0) + "s"
        : "stop OK • " + String(cumminsStateLabelEl(data.gensetState));
    }
    else
      data.lastError = "stop OK • " + String(cumminsStateLabelEl(data.gensetState));
    return true;
  }

  bool cmdStopHard() {
    refreshStatusBlock();
    readCmdRegs();
    uint8_t exc = 0;

    writeHoldRetry(CUMMINS_CMD_START, 0);
    modbusBusGap(80);
    if (!writeHold(CUMMINS_CMD_ESTOP, 1, &exc)) {
      data.lastError = writeError(CUMMINS_CMD_ESTOP, exc);
      return false;
    }
    modbusBusGap(200);
    writeHold(CUMMINS_CMD_ESTOP, 0, &exc);
    modbusBusGap(100);
    writeHoldRetry(CUMMINS_CMD_START, 0);
    modbusBusGap(400);
    refreshStatusBlock();
    readCmdRegs();
    data.lastError = "stop_hard OK • Network Shutdown";
    return true;
  }

  bool refreshStatusBlock() {
    uint16_t a[5];
    if (!readBlock(CUMMINS_REG_CONTROLLER, 5, a)) return false;
    data.controllerType = (uint8_t)a[0];
    data.opMode = (uint8_t)a[1];
    data.gensetState = (uint8_t)a[2];
    data.activeFault = a[3];
    data.faultType = (uint8_t)a[4];
    data.lastUpdate = millis();
    data.valid = true;
    publishPending = true;
    return true;
  }

  void load(Preferences& prefs) {
    enabled = prefs.getBool("modbus_en", MODBUS_DEFAULT_ENABLED);
    profile = (uint8_t)prefs.getUInt("modbus_prof", MODBUS_DEFAULT_PROFILE);
    slaveId = (uint8_t)prefs.getUInt("modbus_id", MODBUS_DEFAULT_SLAVE_ID);
    baud = prefs.getUInt("modbus_baud", MODBUS_DEFAULT_BAUD);
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
  // Interpret probeReg either as a 4xxxx register number (>=40001, subtract the
  // 40001 base) or as a bare wire address (e.g. 8, 9) when a small value is set.
  uint16_t probeAddr() const {
    return probeReg >= 40001 ? (uint16_t)(probeReg - 40001) : probeReg;
  }

  // TTL loopback self-test: jumper TX2<->RX2 (or the module's TXD<->RXD) and
  // this confirms the ESP32 UART + wiring. If it fails with the module's A/B
  // shorted, the MAX485 transceiver is likely dead.
  String loopbackTest() {
    if (!bus) return "no bus";
    const uint8_t pattern[] = { 0xAA, 0x55, 0x01, 0x02, 0x7E, 0x81, 0xF0, 0x0F };
    const size_t n = sizeof(pattern);
    while (bus->available()) bus->read();
    modbusSetTx(MODBUS_DE_PIN, true);
    bus->write(pattern, n);
    bus->flush();
    modbusSetTx(MODBUS_DE_PIN, false);
    uint8_t rx[16];
    size_t got = 0;
    uint32_t start = millis();
    while (got < n && millis() - start < 300) {
      modbusPump();
      while (bus->available() && got < sizeof(rx)) { rx[got++] = (uint8_t)bus->read(); start = millis(); }
      delay(1);
    }
    if (got == 0) return "LOOPBACK: 0 bytes - jumper TX2-RX2 missing, or MAX485 dead";
    int match = 0;
    for (size_t i = 0; i < got && i < n; i++) if (rx[i] == pattern[i]) match++;
    if (match == (int)n) return "LOOPBACK OK: all " + String((int)n) + " bytes echoed - UART & wiring fine";
    return "LOOPBACK partial: " + String(match) + "/" + String((int)n) + " matched (got " + String((int)got) + ") - check baud/noise";
  }

  bool scanBus(uint8_t idMin = 1, uint8_t idMax = 8) {
    static const uint32_t kBauds[] = { 9600, 19200, 38400, 57600, 115200 };
    uint32_t savedBaud = baud;
    uint8_t savedId = slaveId;
    uint16_t addr = probeAddr();
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

  bool readBlock(uint32_t reg40001, uint16_t count, uint16_t* out) {
    return modbusReadHolding(*bus, MODBUS_DE_PIN, slaveId, modbusHoldAddr(reg40001), count, out,
                             MODBUS_READ_TIMEOUT_MS);
  }

  bool readDirect(uint16_t startReg, uint16_t count, uint16_t* out) {
    return modbusReadHolding(*bus, MODBUS_DE_PIN, slaveId, startReg, count, out, 2000);
  }

  bool readHoldOne(uint32_t reg40001, uint16_t* out) {
    return readBlock(reg40001, 1, out);
  }

  bool cmdFaultReset() {
    uint8_t exc = 0;
    if (!writeHold(CUMMINS_CMD_RESET, 1, &exc)) {
      data.lastError = writeError(CUMMINS_CMD_RESET, exc);
      return false;
    }
    modbusBusGap(80);
    return writeHold(CUMMINS_CMD_RESET, 0, &exc);
  }

  bool cmdEstop(bool active) {
    uint8_t exc = 0;
    return writeHold(CUMMINS_CMD_ESTOP, active ? 1 : 0, &exc);
  }

  bool cmdSetOpMode(uint8_t mode) {
    (void)mode;
    refreshStatusBlock();
    // PS0600 register 40010 mirrors the physical panel switch — writes return exc 4.
    data.lastError = "Το 40010 είναι read-only — άλλαξε Off/Auto/Manual μόνο από τον διακόπτη στο panel";
    return false;
  }

  float delayStartTotalSec() const {
    return data.delayStartPreSec;
  }

  float delayStopTotalSec() const {
    return data.delayStopPreSec;
  }

  bool delayCountdownActive() const {
    return data.delayStartActive || data.delayStopActive
      || startDelayArmed || stopDelayArmed;
  }

  void loopTickDelays() {
    if (!enabled || pollPaused || !delayCountdownActive()) return;
    if (millis() - lastDelayPublish < 500) return;
    lastDelayPublish = millis();
    readCmdRegs();
    refreshStatusBlock();
    updateDelayCountdown();
    publishPending = true;
  }

  bool gensetRunning() const {
    return data.gensetState == 8 || data.engineRpm > 100;
  }

  bool gensetStopped() const {
    return data.gensetState <= 1 && data.engineRpm < 50;
  }

  void armStartDelayCountdown() {
    const float total = delayStartTotalSec();
    if (total <= 0) return;
    startDelayArmed = true;
    startDelayT0 = millis();
    data.delayStartActive = true;
    data.delayStartRemainSec = total;
  }

  void armStopDelayCountdown() {
    stopDelayArmed = true;
    stopDelayT0 = millis();
    data.delayStopActive = true;
    data.delayStopRemainSec = delayStopTotalSec();
  }

  void updateDelayCountdown() {
    const uint16_t rs = data.remoteStartReg;
    const uint8_t st = data.gensetState;
    const bool running = gensetRunning();
    const bool stopped = gensetStopped();

    if ((rs == 1 || startDelayArmed) && !running) {
      const float total = delayStartTotalSec();
      if (total > 0 || startDelayArmed) {
        if (!startDelayArmed) armStartDelayCountdown();
        float elapsed = (millis() - startDelayT0) / 1000.0f;
        data.delayStartRemainSec = total > 0 && elapsed < total ? total - elapsed : 0;
        data.delayStartActive = true;
      }
    } else {
      startDelayArmed = false;
      data.delayStartActive = false;
      data.delayStartRemainSec = 0;
    }

    if ((rs == 0 && running) || st == 13 || stopDelayArmed) {
      const float total = delayStopTotalSec();
      if (total > 0 || stopDelayArmed) {
        if (!stopDelayArmed) armStopDelayCountdown();
        float elapsed = (millis() - stopDelayT0) / 1000.0f;
        data.delayStopRemainSec = total > 0 && elapsed < total ? total - elapsed : 0;
        data.delayStopActive = !stopped;
        if (stopped) {
          stopDelayArmed = false;
          data.delayStopActive = false;
          data.delayStopRemainSec = 0;
        }
      } else {
        data.delayStopActive = false;
        data.delayStopRemainSec = 0;
      }
    } else if (stopped || (rs == 1 && running)) {
      stopDelayArmed = false;
      data.delayStopActive = false;
      data.delayStopRemainSec = 0;
    }

    lastRemoteStart = rs;
  }

  bool setDelaySeconds(float start403006, float stop403007) {
    data.lastCmd = "set_delay";
    data.lastCmdOk = false;
    data.lastCmdDetail = "";
    if (!enabled || !bus || profile != MODBUS_PROFILE_PS0600) {
      data.lastError = "delay write unavailable — Modbus OFF";
      data.lastCmdDetail = data.lastError;
      publishPending = true;
      return false;
    }
    if (start403006 < 0 && stop403007 < 0) {
      data.lastError = "no delay values";
      return false;
    }

    pollPaused = true;
    flushBus();
    modbusBusGap(50);
    bool ok = true;
    bool usedTdes = false;
    bool usedTdec = false;
    String note;

    if (start403006 >= 0) {
      if (start403006 > 300) start403006 = 300;
      uint16_t raw = (uint16_t)(start403006 + 0.5f);
      uint8_t exc = 0;
      if (writeHold(CUMMINS_REG_TIME_DELAY_START, raw, &exc)) {
        if (!writeHoldRetry(CUMMINS_REG_DELAY_START, 0)) ok = false;
      } else if (exc == 4) {
        uint16_t tdes = (uint16_t)(start403006 * 10.0f + 0.5f);
        if (!writeHoldRetry(CUMMINS_REG_DELAY_START, tdes)) ok = false;
        else { usedTdes = true; note += " start→TDES"; }
      } else {
        ok = false;
      }
      modbusBusGap(80);
    }

    if (stop403007 >= 0) {
      if (stop403007 > 600) stop403007 = 600;
      uint16_t raw = (uint16_t)(stop403007 + 0.5f);
      uint8_t exc = 0;
      if (writeHold(CUMMINS_REG_TIME_DELAY_STOP, raw, &exc)) {
        if (!writeHoldRetry(CUMMINS_REG_DELAY_STOP, 0)) ok = false;
      } else if (exc == 4) {
        uint16_t tdec = (uint16_t)(stop403007 * 10.0f + 0.5f);
        if (!writeHoldRetry(CUMMINS_REG_DELAY_STOP, tdec)) ok = false;
        else { usedTdec = true; note += " stop→TDEC"; }
      } else {
        ok = false;
      }
      modbusBusGap(80);
    }

    uint16_t d[2];
    if (!readBlock(CUMMINS_REG_DELAY_START, 2, d)) {
      ok = false;
      if (!data.lastError.length()) data.lastError = "TDES/TDEC readback failed";
    } else {
      data.delayStartSec = cumminsNaU(d[0]) ? 0 : d[0] * 0.1f;
      data.delayStopSec = cumminsNaU(d[1]) ? 0 : d[1] * 0.1f;
    }

    uint16_t td[2];
    if (!readBlock(CUMMINS_REG_TIME_DELAY_START, 2, td)) {
      ok = false;
      if (!data.lastError.length()) data.lastError = "403006/403007 readback failed";
    } else {
      data.delayStartPreSec = cumminsNaU(td[0]) ? 0 : (float)td[0];
      data.delayStopPreSec = cumminsNaU(td[1]) ? 0 : (float)td[1];
    }

    if (start403006 >= 0 && ok) {
      float got = usedTdes ? data.delayStartSec : data.delayStartPreSec;
      if (fabsf(got - start403006) > 0.5f) {
        ok = false;
        data.lastError = (usedTdes ? "TDES" : "403006") + String(" readback ")
          + String(got, 0) + "s != " + String(start403006, 0) + "s";
      }
    }
    if (stop403007 >= 0 && ok) {
      float got = usedTdec ? data.delayStopSec : data.delayStopPreSec;
      if (fabsf(got - stop403007) > 0.5f) {
        ok = false;
        data.lastError = (usedTdec ? "TDEC" : "403007") + String(" readback ")
          + String(got, 0) + "s != " + String(stop403007, 0) + "s";
      }
    }

    data.lastCmdOk = ok;
    data.lastCmdDetail = ok
      ? "start=" + String(delayStartTotalSec(), 0) + "s stop=" + String(delayStopTotalSec(), 0) + "s" + note
      : data.lastError;
    publishPending = true;
    pollPaused = false;
    return ok;
  }

  bool runGensetCmd(const char* action) {
    data.lastCmd = action ? action : "";
    data.lastCmdOk = false;
    data.lastCmdDetail = "";
    if (!enabled || !bus || !action || profile != MODBUS_PROFILE_PS0600) {
      data.lastError = "genset cmd unavailable — Modbus OFF";
      data.lastCmdDetail = data.lastError;
      data.lastCmdOk = false;
      publishPending = true;
      return false;
    }

    pollPaused = true;
    flushBus();
    modbusBusGap(50);
    refreshStatusBlock();
    readCmdRegs();

    bool ok = false;
    if (strcmp(action, "start") == 0) {
      ok = cmdStart();
      if (ok) {
        armStartDelayCountdown();
        if (data.gensetState >= 4)
          data.lastError = "start OK • reg40300=1 • " + String(cumminsStateLabelEl(data.gensetState));
        else
          data.lastError = "40300=1 OK — περίμενε 10–30s (Precrank → Running)";
      }
    } else if (strcmp(action, "stop") == 0) {
      ok = cmdStop();
      if (ok && (data.gensetState == 8 || data.engineRpm > 100 || data.gensetState == 13))
        armStopDelayCountdown();
    } else if (strcmp(action, "stop_hard") == 0) {
      ok = cmdStopHard();
      stopDelayArmed = false;
      data.delayStopActive = false;
      data.delayStopRemainSec = 0;
    } else if (strcmp(action, "reset") == 0) {
      ok = cmdFaultReset();
      if (!ok && data.lastError.length() == 0) data.lastError = "fault reset write 40301 failed";
    } else if (strcmp(action, "estop_on") == 0) {
      ok = cmdEstop(true);
      if (!ok) data.lastError = "estop write 40302 failed";
    } else if (strcmp(action, "estop_off") == 0) {
      ok = cmdEstop(false);
      if (!ok) data.lastError = "estop release write 40302 failed";
    } else if (strcmp(action, "mode_off") == 0) {
      ok = cmdSetOpMode(0);
    } else if (strcmp(action, "mode_auto") == 0) {
      ok = cmdSetOpMode(1);
    } else if (strcmp(action, "mode_manual") == 0) {
      ok = cmdSetOpMode(2);
    } else {
      pollPaused = false;
      data.lastError = "unknown genset cmd";
      return false;
    }

    refreshStatusBlock();
    readCmdRegs();
    updateDelayCountdown();
    data.lastCmdOk = ok;
    data.lastCmdDetail = data.lastError;
    if (!ok && data.lastCmdDetail.length() == 0)
      data.lastCmdDetail = "εντολή απέτυχε";
    publishPending = true;
    pollPaused = false;
    return ok;
  }

  bool pollEntesOnce() {
#ifndef ESP32_SLIM_BUILD
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
    data.pollComplete = true;
    data.lastUpdate = millis();
    data.lastError = "";
    publishPending = true;
    return true;
#else
    data.lastError = "ENTES profile disabled on this build";
    return false;
#endif
  }

  void pollReset() { pollStep = 0; data.pollComplete = false; }

  // One Modbus transaction. Returns true when a full PS0600 cycle just finished.
  bool pollStepOnce() {
    if (!enabled || !bus || pollPaused) return false;
    if (profile == MODBUS_PROFILE_ENTES) return pollEntesOnce();

    switch (pollStep) {
      case 0: {
        uint16_t a[5];
        if (!readBlock(CUMMINS_REG_CONTROLLER, 5, a)) {
          data.lastError = "modbus 40009 - check RS485 A/B, baud, slave ID";
          if (++pollFailStreak >= 5) data.valid = false;
          return false;
        }
        pollFailStreak = 0;
        data.controllerType = (uint8_t)a[0];
        data.opMode = (uint8_t)a[1];
        data.gensetState = (uint8_t)a[2];
        data.activeFault = a[3];
        data.faultType = (uint8_t)a[4];
        markUpdated();
        pollStep = 1;
        modbusBusGap();
        return false;
      }
      case 1: {
        uint16_t f[2];
        if (readBlock(CUMMINS_REG_NFPA_FAULT, 2, f))
          data.nfpaFault = ((uint32_t)f[0] << 16) | f[1];
        readCmdRegs();
        updateDelayCountdown();
        pollStep = 2;
        modbusBusGap();
        return false;
      }
      case 2: {
        uint16_t v[3];
        if (!readBlock(CUMMINS_REG_VOLT_L1N, 3, v)) {
          data.lastError = "modbus 40018";
        } else {
          data.voltL1N = cumminsNaU(v[0]) ? 0 : (float)v[0];
          data.voltL2N = cumminsNaU(v[1]) ? 0 : (float)v[1];
          data.voltL3N = cumminsNaU(v[2]) ? 0 : (float)v[2];
          markUpdated();
        }
        pollStep = 3;
        modbusBusGap();
        return false;
      }
      case 3: {
        uint16_t ll[3];
        if (!readBlock(CUMMINS_REG_VOLT_L1L2, 3, ll)) {
          data.lastError = "modbus 40022";
        } else {
          data.voltL1L2 = cumminsNaU(ll[0]) ? 0 : (float)ll[0];
          data.voltL2L3 = cumminsNaU(ll[1]) ? 0 : (float)ll[1];
          data.voltL3L1 = cumminsNaU(ll[2]) ? 0 : (float)ll[2];
          markUpdated();
        }
        pollStep = 4;
        modbusBusGap();
        return false;
      }
      case 4: {
        uint16_t c[3];
        if (!readBlock(CUMMINS_REG_CURR_L1, 3, c)) {
          data.lastError = "modbus 40026";
        } else {
          data.currL1 = cumminsNaU(c[0]) ? 0 : (float)c[0];
          data.currL2 = cumminsNaU(c[1]) ? 0 : (float)c[1];
          data.currL3 = cumminsNaU(c[2]) ? 0 : (float)c[2];
          data.currAvg = (data.currL1 + data.currL2 + data.currL3) / 3.0f;
          data.voltAvgLL = (data.voltL1L2 + data.voltL2L3 + data.voltL3L1) / 3.0f;
          markUpdated();
        }
        pollStep = 5;
        modbusBusGap();
        return false;
      }
      case 5: {
        uint16_t k[4];
        if (!readBlock(CUMMINS_REG_KW_L1, 4, k)) {
          data.lastError = "modbus 40031";
        } else {
          data.kwL1 = cumminsNaS((int16_t)k[0]) ? 0 : (int16_t)k[0];
          data.kwL2 = cumminsNaS((int16_t)k[1]) ? 0 : (int16_t)k[1];
          data.kwL3 = cumminsNaS((int16_t)k[2]) ? 0 : (int16_t)k[2];
          data.kwTotal = cumminsNaS((int16_t)k[3]) ? 0 : (int16_t)k[3];
          markUpdated();
        }
        pollStep = 6;
        modbusBusGap();
        return false;
      }
      case 6: {
        uint16_t p[5];
        if (!readBlock(CUMMINS_REG_KVA_L1, 5, p)) {
          data.lastError = "modbus 40040";
        } else {
          data.kvaL1 = cumminsNaU(p[0]) ? 0 : (float)p[0];
          data.kvaL2 = cumminsNaU(p[1]) ? 0 : (float)p[1];
          data.kvaL3 = cumminsNaU(p[2]) ? 0 : (float)p[2];
          data.kvaTotal = cumminsNaU(p[3]) ? 0 : (float)p[3];
          data.frequency = cumminsNaU(p[4]) ? 0 : p[4] * 0.01f;
          markUpdated();
        }
        pollStep = 7;
        modbusBusGap();
        return false;
      }
      case 7: {
        uint16_t l[3];
        if (!readBlock(CUMMINS_REG_LOAD_L1, 3, l)) {
          data.lastError = "modbus 40058";
        } else {
          data.loadL1Pct = cumminsNaU(l[0]) ? 0 : l[0] * 0.1f;
          data.loadL2Pct = cumminsNaU(l[1]) ? 0 : l[1] * 0.1f;
          data.loadL3Pct = cumminsNaU(l[2]) ? 0 : l[2] * 0.1f;
          markUpdated();
        }
        pollStep = 8;
        modbusBusGap();
        return false;
      }
      case 8: {
        uint16_t e[2];
        if (!readBlock(CUMMINS_REG_BATTERY_V, 2, e)) {
          data.lastError = "modbus 40061";
        } else {
          data.batteryV = cumminsNaU(e[0]) ? 0 : e[0] * 0.001f;
          data.oilKpa = cumminsNaU(e[1]) ? 0 : (e[1] * 0.1f) * 6.894757f;
          markUpdated();
        }
        pollStep = 9;
        modbusBusGap();
        return false;
      }
      case 9: {
        uint16_t t[1];
        if (!readBlock(CUMMINS_REG_COOLANT_F, 1, t)) {
          data.lastError = "modbus 40064";
        } else {
          int16_t rawF = (int16_t)t[0];
          data.coolantC = cumminsNaS(rawF) ? 0 : ((rawF * 0.1f) - 32.0f) * (5.0f / 9.0f);
          markUpdated();
        }
        pollStep = 10;
        modbusBusGap();
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
        uint32_t rtRaw = ((uint32_t)r[2] << 16) | r[3];
        data.runtimeHours = rtRaw * 0.05f;
        data.runTimeSec = (uint32_t)(data.runtimeHours * 3600.0f);
        pollStep = 11;
        modbusBusGap();
        return false;
      }
      case 11: {
        uint16_t d[2];
        if (readBlock(CUMMINS_REG_DELAY_START, 2, d)) {
          data.delayStartSec = cumminsNaU(d[0]) ? 0 : d[0] * 0.1f;
          data.delayStopSec = cumminsNaU(d[1]) ? 0 : d[1] * 0.1f;
          markUpdated();
        }
        uint16_t td[2];
        if (readBlock(CUMMINS_REG_TIME_DELAY_START, 2, td)) {
          data.delayStartPreSec = cumminsNaU(td[0]) ? 0 : (float)td[0];
          data.delayStopPreSec = cumminsNaU(td[1]) ? 0 : (float)td[1];
          markUpdated();
        }
        updateDelayCountdown();
        data.pollComplete = true;
        data.lastError = "";
        markUpdated(true);
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
    if (profile == MODBUS_PROFILE_ENTES) return pollEntesOnce();
    flushBus();
    for (uint8_t i = 0; i < 16; i++) {
      if (pollStepOnce()) return true;
      modbusPump();
    }
    return data.valid;
  }

  void poll() {
    if (!enabled || pollPaused) return;
    if (profile == MODBUS_PROFILE_ENTES) {
      if (millis() - lastPoll < pollIntervalMs) return;
      lastPoll = millis();
      pollEntesOnce();
      publishPending = true;
      return;
    }

    // Mid-cycle: continue immediately (no interval wait).
    if (pollStep == 0) {
      if (lastPoll != 0 && millis() - lastPoll < pollIntervalMs) return;
      lastPoll = millis();
      flushBus();
    }

    uint32_t budgetStart = millis();
    do {
      if (pollStepOnce()) break;
      modbusPump();
    } while (pollStep != 0 && millis() - budgetStart < kPollBudgetMs);

    if (data.valid && data.lastUpdate && millis() - data.lastUpdate > 120000) {
      data.valid = false;
      data.pollComplete = false;
      data.lastError = "modbus timeout";
    }
  }
};
