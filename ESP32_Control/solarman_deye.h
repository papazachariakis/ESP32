#pragma once
// Solarman V5 TCP client for DEYE 3-phase hybrid (deye_p3).
// Frame layout matches HA custom_components/solarman/pysolarman (not upstream pysolarmanv5).
// Non-blocking: at most one Modbus request per loop() tick.
#include <WiFi.h>
#include <Arduino.h>

#ifndef DEYE_LOGGER_HOST
#define DEYE_LOGGER_HOST "192.168.99.39"
#endif
#ifndef DEYE_LOGGER_PORT
#define DEYE_LOGGER_PORT 8899
#endif
#ifndef DEYE_LOGGER_SERIAL
#define DEYE_LOGGER_SERIAL 3141362597UL
#endif
#ifndef DEYE_MB_SLAVE
#define DEYE_MB_SLAVE 1
#endif
#ifndef DEYE_STEP_MS
#define DEYE_STEP_MS 100
#endif
#ifndef DEYE_BOOT_GRACE_MS
#define DEYE_BOOT_GRACE_MS 45000
#endif

// Defined in ESP32_Control.ino — keeps cloud MQTT alive during Solarman TCP waits.
void pvxPumpLive();

struct DeyeLive {
  bool valid = false;
  uint32_t ageMs = 0;
  uint16_t state = 0;
  bool power_on = false;   // switch.inverter  reg 0x0050
  bool off_grid = false;   // switch.inverter_off_grid  reg 0x00B3
  bool switches_ok = false;
  int32_t pv1_w = 0, pv2_w = 0, pv3_w = 0, pv4_w = 0, pv_w = 0;
  int32_t load_w = 0, grid_w = 0, external_w = 0;
  int32_t ct1_w = 0, ct2_w = 0, ct3_w = 0;
  int32_t battery_w = 0;
  float battery_v = 0, battery_a = 0, battery_temp_c = 0;
  uint16_t battery_soc = 0;
  float today_pv_kwh = 0, today_load_kwh = 0;
  float today_import_kwh = 0, today_export_kwh = 0;
  float today_bat_charge_kwh = 0, today_bat_discharge_kwh = 0;
  String lastError;
  String lastCmdError;
};

class SolarmanDeye {
 public:
  DeyeLive data;
  String host = DEYE_LOGGER_HOST;
  uint16_t port = DEYE_LOGGER_PORT;
  uint32_t loggerSerial = DEYE_LOGGER_SERIAL;
  uint8_t slave = DEYE_MB_SLAVE;
  bool enabled = true;
  volatile bool holdPoll = false;  // pause live reads during switch writes

  void begin() {
    _seq = (uint8_t)((millis() & 0xFE) + 1);
    _step = 0;
    _bootAt = millis();
    _lastOk = 0;
    _lastStep = 0;
  }

  void loop() {
    if (!enabled || WiFi.status() != WL_CONNECTED) return;
    if (holdPoll) return;
    if (millis() - _bootAt < DEYE_BOOT_GRACE_MS) return;
    if (millis() - _lastStep < DEYE_STEP_MS) return;
    _lastStep = millis();
    stepOnce();
    if (data.valid && _lastOk && (millis() - _lastOk > 30000)) {
      data.valid = false;
      if (!data.lastError.length()) data.lastError = "stale";
    }
  }

  void refreshAge() {
    if (!data.valid) return;
    data.ageMs = millis() - _lastOk;
  }

  // deye_p3: On/Off switch @ 0x0050 (1=on, 0=off)
  bool setPowerOn(bool on) {
    bool ok = writeHoldingRetry(0x0050, on ? 1 : 0);
    if (ok) {
      data.power_on = on;
      data.lastCmdError = "";
    } else if (!data.lastCmdError.length()) {
      data.lastCmdError = "power write fail";
    }
    return ok;
  }

  // deye_p3: Off Grid @ 0x00B3 (1=on, 0=off)
  bool setOffGrid(bool on) {
    bool ok = writeHoldingRetry(0x00B3, on ? 1 : 0);
    if (ok) {
      data.off_grid = on;
      data.lastCmdError = "";
    } else if (!data.lastCmdError.length()) {
      data.lastCmdError = "off_grid write fail";
    }
    return ok;
  }

  // Confirm register value with fresh reads (no cached optimistic state).
  bool confirmRegister(uint16_t addr, uint16_t expect) {
    uint16_t regs[2] = {0xFFFF, 0};
    for (int i = 0; i < 3; i++) {
      if (readHolding(addr, 1, regs) && regs[0] == expect) return true;
      delay(100);
    }
    return false;
  }

  // After UI/HA switch writes, keep published state for a few seconds
  // (Solarman register 0x00B3 can disagree briefly / fail under contention).
  void holdSwitchState(uint32_t ms = 20000) {
    _switchHoldUntil = millis() + ms;
  }

 private:
  WiFiClient _cli;
  uint8_t _seq = 1;
  uint8_t _step = 0;
  unsigned long _bootAt = 0, _lastOk = 0, _lastStep = 0;
  unsigned long _switchHoldUntil = 0;

  static uint16_t mbCrc(const uint8_t* d, size_t n) {
    uint16_t c = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
      c ^= d[i];
      for (int b = 0; b < 8; b++) c = (c & 1) ? (c >> 1) ^ 0xA001 : (c >> 1);
    }
    return c;
  }

  void stepOnce() {
    uint16_t regs[8];
    switch (_step) {
      case 0:
        if (readHolding(0x024A, 6, regs)) {
          int16_t rt = (int16_t)regs[0];
          float t = rt * 0.1f;
          if (t > 80 || t < -40) t = (rt - 1000) * 0.1f;
          data.battery_temp_c = t;
          data.battery_v = regs[1] * 0.01f;
          data.battery_soc = regs[2];
          data.battery_w = (int16_t)regs[4];
          data.battery_a = ((int16_t)regs[5]) * 0.01f;
          markOk();
        } else data.lastError = "battery";
        break;
      case 1:
        if (readHolding(0x028D, 1, regs)) {
          data.load_w = (int16_t)regs[0];
          if (data.load_w < 0) data.load_w = -data.load_w;
          markOk();
        } else data.lastError = "load";
        break;
      case 2:
        if (readHolding(0x02A0, 2, regs)) {
          data.pv1_w = (int16_t)regs[0];
          data.pv2_w = (int16_t)regs[1];
          data.pv_w = data.pv1_w + data.pv2_w + data.pv3_w + data.pv4_w;
          markOk();
        } else data.lastError = "pv";
        break;
      case 3:
        if (readHolding(0x0268, 4, regs)) {
          data.ct1_w = (int16_t)regs[0];
          data.ct2_w = (int16_t)regs[1];
          data.ct3_w = (int16_t)regs[2];
          data.external_w = (int16_t)regs[3];
          markOk();
        } else data.lastError = "ct";
        break;
      case 4:
        if (readHolding(0x0271, 1, regs)) {
          data.grid_w = (int16_t)regs[0];
          markOk();
        } else data.lastError = "grid";
        break;
      case 5:
        if (readHolding(0x01F4, 1, regs)) {
          data.state = regs[0];
          markOk();
        } else data.lastError = "state";
        break;
      case 6:
        if (readHolding(0x0202, 2, regs)) {
          data.today_bat_charge_kwh = regs[0] * 0.1f;
          data.today_bat_discharge_kwh = regs[1] * 0.1f;
          markOk();
        } else data.lastError = "e_bat";
        break;
      case 7:
        if (readHolding(0x0208, 2, regs)) {
          data.today_import_kwh = regs[0] * 0.1f;
          data.today_export_kwh = regs[1] * 0.1f;
          markOk();
        } else data.lastError = "e_grid";
        break;
      case 8:
        if (readHolding(0x020E, 1, regs)) {
          data.today_load_kwh = regs[0] * 0.1f;
          markOk();
        } else data.lastError = "e_load";
        break;
      case 9:
        if (readHolding(0x0211, 1, regs)) {
          data.today_pv_kwh = regs[0] * 0.1f;
          markOk();
        } else data.lastError = "e_pv";
        break;
      case 10:
        if (millis() < _switchHoldUntil) { markOk(); break; }
        if (readHolding(0x0050, 1, regs)) {
          data.power_on = (regs[0] != 0);
          data.switches_ok = true;
          markOk();
        } else data.lastError = "sw_pwr";
        break;
      case 11:
        if (millis() < _switchHoldUntil) { markOk(); break; }
        if (readHolding(0x00B3, 1, regs)) {
          data.off_grid = (regs[0] != 0);
          data.switches_ok = true;
          markOk();
        } else data.lastError = "sw_og";
        break;
      default:
        _step = 0;
        return;
    }
    _step++;
    // Hot path: battery/load/pv/ct/grid (0–4). Cold energy/state/switches less often
    // so live watts refresh faster without starving Solarman.
    if (_step == 5) {
      static uint8_t coldGate = 0;
      if ((++coldGate % 4) != 0) _step = 0;
    }
    if (_step > 11) _step = 0;
  }

  void markOk() {
    data.valid = true;
    data.lastError = "";
    _lastOk = millis();
    data.ageMs = 0;
  }

  bool extractRegs(const uint8_t* buf, size_t got, uint8_t seq, uint16_t count, uint16_t* out) {
    // Prefer HA fixed offset: modbus at byte 25, checksum/end after.
    if (got >= 25 + 3 + count * 2 + 2 + 2) {
      for (size_t i = 0; i + 25 + 5 < got; i++) {
        if (buf[i] != 0xA5) continue;
        if (buf[i + 3] != 0x10) continue;
        // response control = request(0x45) - 0x30 = 0x15
        if (buf[i + 4] != 0x15) continue;
        if (buf[i + 5] != seq) continue;
        const uint8_t* mb = &buf[i + 25];
        size_t remain = got - (i + 25);
        if (remain < (size_t)(3 + count * 2 + 2)) continue;
        if (mb[0] != slave || mb[1] != 0x03 || mb[2] != count * 2) continue;
        uint16_t crc = mbCrc(mb, 3 + count * 2);
        uint16_t gotCrc = mb[3 + count * 2] | (uint16_t)(mb[4 + count * 2] << 8);
        if (crc != gotCrc) continue;
        for (uint16_t r = 0; r < count; r++)
          out[r] = (uint16_t)((mb[3 + r * 2] << 8) | mb[4 + r * 2]);
        return true;
      }
    }
    // Fallback: scan for Modbus FC03 + CRC anywhere after A5.
    for (size_t i = 0; i + 12 < got; i++) {
      if (buf[i] != 0xA5) continue;
      const uint8_t need = (uint8_t)(3 + count * 2 + 2);
      for (size_t m = i + 11; m + need <= got; m++) {
        if (buf[m] != slave || buf[m + 1] != 0x03 || buf[m + 2] != count * 2) continue;
        uint16_t crc = mbCrc(&buf[m], 3 + count * 2);
        uint16_t gotCrc = buf[m + 3 + count * 2] | (uint16_t)(buf[m + 4 + count * 2] << 8);
        if (crc != gotCrc) continue;
        for (uint16_t r = 0; r < count; r++)
          out[r] = (uint16_t)((buf[m + 3 + r * 2] << 8) | buf[m + 4 + r * 2]);
        return true;
      }
    }
    return false;
  }

  // Keep logger TCP warm — reconnect-per-step was ~200–400ms and capped live MQTT ≪10Hz.
  bool ensureTcp(unsigned timeoutMs = 800) {
    if (_cli.connected()) return true;
    _cli.stop();
    return _cli.connect(host.c_str(), port, timeoutMs);
  }

  void drainTcp() {
    unsigned long t0 = millis();
    while (_cli.available() && (millis() - t0 < 30)) {
      (void)_cli.read();
    }
  }

  bool readHolding(uint16_t start, uint16_t count, uint16_t* out) {
    if (count == 0 || count > 8) return false;
    uint8_t mb[8] = {
      slave, 0x03,
      (uint8_t)(start >> 8), (uint8_t)(start & 0xFF),
      (uint8_t)(count >> 8), (uint8_t)(count & 0xFF), 0, 0
    };
    uint16_t crc = mbCrc(mb, 6);
    mb[6] = crc & 0xFF;
    mb[7] = (crc >> 8) & 0xFF;

    // HA solarman request:
    // A5 | len=15+mb | 10 45 | seq_lo seq_hi | serial_le | 02 | 0000 | 12*00 | mb | chk | 15
    uint8_t frame[64];
    size_t idx = 0;
    uint8_t seq = _seq;
    _seq = (uint8_t)((_seq + 1) & 0xFF);
    if (!_seq) _seq = 1;

    uint16_t length = (uint16_t)(15 + 8);
    frame[idx++] = 0xA5;
    frame[idx++] = length & 0xFF;
    frame[idx++] = (length >> 8) & 0xFF;
    frame[idx++] = 0x10;
    frame[idx++] = 0x45;
    frame[idx++] = seq;
    frame[idx++] = 0x00;
    frame[idx++] = loggerSerial & 0xFF;
    frame[idx++] = (loggerSerial >> 8) & 0xFF;
    frame[idx++] = (loggerSerial >> 16) & 0xFF;
    frame[idx++] = (loggerSerial >> 24) & 0xFF;
    frame[idx++] = 0x02;
    frame[idx++] = 0x00;
    frame[idx++] = 0x00;
    for (int i = 0; i < 12; i++) frame[idx++] = 0;
    for (int i = 0; i < 8; i++) frame[idx++] = mb[i];
    uint8_t chk = 0;
    for (size_t i = 1; i < idx; i++) chk += frame[i];
    frame[idx++] = chk;
    frame[idx++] = 0x15;

    if (!ensureTcp()) return false;
    drainTcp();
    if (_cli.write(frame, idx) != (int)idx) {
      _cli.stop();
      return false;
    }

    unsigned long t0 = millis();
    uint8_t buf[320];
    size_t got = 0;
    // Short read timeout keeps main loop free for 100ms cloud publishes.
    while (millis() - t0 < 500) {
      while (_cli.available() && got < sizeof(buf)) buf[got++] = (uint8_t)_cli.read();
      if (got >= 30 && extractRegs(buf, got, seq, count, out)) {
        return true;  // keep TCP open
      }
      delay(1);
      yield();
      pvxPumpLive();
    }
    _cli.stop();
    return false;
  }

  bool extractWriteOk(const uint8_t* buf, size_t got, uint8_t expectFc, uint16_t addr, uint16_t value) {
    // Accept FC06 echo or FC16 ack (addr + quantity=1)
    for (size_t i = 0; i + 12 < got; i++) {
      if (buf[i] != 0xA5) continue;
      for (size_t m = i + 11; m + 8 <= got; m++) {
        if (buf[m] != slave) continue;
        uint8_t fc = buf[m + 1];
        if (fc != 0x06 && fc != 0x10 && fc != (expectFc | 0x80)) continue;
        if (fc & 0x80) continue;  // exception
        uint16_t a = (uint16_t)((buf[m + 2] << 8) | buf[m + 3]);
        if (a != addr) continue;
        uint16_t crc = mbCrc(&buf[m], 6);
        uint16_t gotCrc = buf[m + 6] | (uint16_t)(buf[m + 7] << 8);
        if (crc != gotCrc) continue;
        if (fc == 0x06) {
          uint16_t v = (uint16_t)((buf[m + 4] << 8) | buf[m + 5]);
          if (v == value) return true;
        } else if (fc == 0x10) {
          uint16_t q = (uint16_t)((buf[m + 4] << 8) | buf[m + 5]);
          if (q >= 1) return true;
        }
      }
    }
    return false;
  }

  bool sendMbFrame(const uint8_t* mb, size_t mbLen, uint8_t& seqOut, uint8_t* resp, size_t respCap, size_t& gotOut, unsigned timeoutMs) {
    gotOut = 0;
    uint8_t frame[80];
    size_t idx = 0;
    uint8_t seq = _seq;
    _seq = (uint8_t)((_seq + 1) & 0xFF);
    if (!_seq) _seq = 1;
    seqOut = seq;

    uint16_t length = (uint16_t)(15 + mbLen);
    frame[idx++] = 0xA5;
    frame[idx++] = length & 0xFF;
    frame[idx++] = (length >> 8) & 0xFF;
    frame[idx++] = 0x10;
    frame[idx++] = 0x45;
    frame[idx++] = seq;
    frame[idx++] = 0x00;
    frame[idx++] = loggerSerial & 0xFF;
    frame[idx++] = (loggerSerial >> 8) & 0xFF;
    frame[idx++] = (loggerSerial >> 16) & 0xFF;
    frame[idx++] = (loggerSerial >> 24) & 0xFF;
    frame[idx++] = 0x02;
    frame[idx++] = 0x00;
    frame[idx++] = 0x00;
    for (int i = 0; i < 12; i++) frame[idx++] = 0;
    for (size_t i = 0; i < mbLen; i++) frame[idx++] = mb[i];
    uint8_t chk = 0;
    for (size_t i = 1; i < idx; i++) chk += frame[i];
    frame[idx++] = chk;
    frame[idx++] = 0x15;

    _cli.stop();
    delay(20);
    if (!_cli.connect(host.c_str(), port, 2000)) {
      data.lastCmdError = "tcp connect";
      return false;
    }
    if (_cli.write(frame, idx) != (int)idx) {
      _cli.stop();
      data.lastCmdError = "tcp write";
      return false;
    }
    unsigned long t0 = millis();
    while (millis() - t0 < timeoutMs) {
      while (_cli.available() && gotOut < respCap) resp[gotOut++] = (uint8_t)_cli.read();
      if (gotOut >= 28) break;
      delay(2);
      yield();
    }
    _cli.stop();
    return gotOut > 0;
  }

  bool writeHoldingOnce(uint16_t addr, uint16_t value, bool useFc16) {
    uint8_t mb[16];
    size_t mbLen = 0;
    uint8_t fc = useFc16 ? 0x10 : 0x06;
    if (useFc16) {
      mb[0] = slave; mb[1] = 0x10;
      mb[2] = (uint8_t)(addr >> 8); mb[3] = (uint8_t)(addr & 0xFF);
      mb[4] = 0x00; mb[5] = 0x01;
      mb[6] = 0x02;
      mb[7] = (uint8_t)(value >> 8); mb[8] = (uint8_t)(value & 0xFF);
      uint16_t crc = mbCrc(mb, 9);
      mb[9] = crc & 0xFF; mb[10] = (crc >> 8) & 0xFF;
      mbLen = 11;
    } else {
      mb[0] = slave; mb[1] = 0x06;
      mb[2] = (uint8_t)(addr >> 8); mb[3] = (uint8_t)(addr & 0xFF);
      mb[4] = (uint8_t)(value >> 8); mb[5] = (uint8_t)(value & 0xFF);
      uint16_t crc = mbCrc(mb, 6);
      mb[6] = crc & 0xFF; mb[7] = (crc >> 8) & 0xFF;
      mbLen = 8;
    }

    uint8_t seq = 0;
    uint8_t buf[200];
    size_t got = 0;
    bool gotResp = sendMbFrame(mb, mbLen, seq, buf, sizeof(buf), got, 2500);
    bool acked = gotResp && extractWriteOk(buf, got, fc, addr, value);

    // NEVER trust ACK alone — logger contention often yields false ACKs.
    delay(120);
    if (confirmRegister(addr, value)) {
      data.lastCmdError = "";
      return true;
    }
    if (!gotResp) data.lastCmdError = "write timeout";
    else if (acked) data.lastCmdError = "ack but verify fail";
    else data.lastCmdError = "bad write ack";
    return false;
  }

  bool writeHoldingRetry(uint16_t addr, uint16_t value) {
    holdPoll = true;
    delay(300);  // let HA/ESP release logger socket
    bool ok = false;
    for (int i = 0; i < 8 && !ok; i++) {
      // Prefer FC16 (Deye), then FC06.
      if (writeHoldingOnce(addr, value, true)) { ok = true; break; }
      delay(150 + i * 60);
      if (writeHoldingOnce(addr, value, false)) { ok = true; break; }
      delay(200 + i * 80);
    }
    // Final confirmation pass
    if (ok && !confirmRegister(addr, value)) {
      ok = false;
      data.lastCmdError = "final verify fail";
    }
    holdPoll = false;
    return ok;
  }

  bool writeHolding(uint16_t addr, uint16_t value) {
    return writeHoldingRetry(addr, value);
  }
};
