#pragma once
// Cloud MQTT publisher for Vercel PVX dashboard (public EMQX, no HA/PC needed).
// Commands: pvx/<deviceId>/cmd  JSON {"password":"...","inverter":true,"off_grid":false}
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "solarman_deye.h"
#include "bms_manager.h"
#include "ha_api.h"

#ifndef PVX_CLOUD_BROKER
#define PVX_CLOUD_BROKER "broker.emqx.io"
#endif
#ifndef PVX_CLOUD_PORT
#define PVX_CLOUD_PORT 1883
#endif
#ifndef PVX_CLOUD_PUBLISH_MS
#define PVX_CLOUD_PUBLISH_MS 100
#endif

class PvxCloud {
 public:
  bool enabled = true;
  String broker = PVX_CLOUD_BROKER;
  uint16_t port = PVX_CLOUD_PORT;
  String topicPrefix;  // e.g. pvx/38182B8BD5CC
  bool connected = false;

  void begin(const String& deviceId) {
    topicPrefix = String("pvx/") + deviceId;
    _self = this;
    _mqtt.setClient(_wifi);
    _mqtt.setServer(broker.c_str(), port);
    _mqtt.setBufferSize(2048);
    _mqtt.setKeepAlive(30);
    _mqtt.setCallback(_staticCb);
  }

  void loop(SolarmanDeye& deye, BmsManager& bms, const String& deviceId, const char* fw) {
    if (!enabled || WiFi.status() != WL_CONNECTED) return;
    _deye = &deye;
    if (!_mqtt.connected()) {
      if (millis() - _lastConn < 4000) return;
      _lastConn = millis();
      String cid = String("pvx-") + deviceId;
      connected = _mqtt.connect(cid.c_str());
      if (connected) {
        String cmd = topicPrefix + "/cmd";
        _mqtt.subscribe(cmd.c_str(), 0);
        Serial.printf("PVX cloud MQTT connected, sub %s\n", cmd.c_str());
      } else {
        Serial.println("PVX cloud MQTT connect fail");
      }
      return;
    }
    _mqtt.loop();
    applyPending(deye);
    syncSwitchesFromHa(deye);
    if (millis() - _lastPub < PVX_CLOUD_PUBLISH_MS) return;
    _lastPub = millis();
    publish(deye, bms, deviceId, fw);
  }

 private:
  WiFiClient _wifi;
  PubSubClient _mqtt;
  unsigned long _lastConn = 0;
  unsigned long _lastPub = 0;
  SolarmanDeye* _deye = nullptr;
  static PvxCloud* _self;

  bool _pendInverter = false;
  bool _pendOffGrid = false;
  bool _valInverter = false;
  bool _valOffGrid = false;
  String _lastCmdStatus;
  unsigned long _lastHaSync = 0;
  bool _haSyncFlip = false;

  static void _staticCb(char* topic, byte* payload, unsigned int len) {
    if (_self) _self->onMessage(topic, payload, len);
  }

  void onMessage(char* topic, byte* payload, unsigned int len) {
    String t(topic);
    if (!t.endsWith("/cmd")) return;
    StaticJsonDocument<384> doc;
    DeserializationError err = deserializeJson(doc, payload, len);
    if (err) {
      _lastCmdStatus = "bad json";
      return;
    }
    const char* pw = doc["password"] | "";
    if (strcmp(pw, DEVICE_CMD_PASSWORD) != 0) {
      _lastCmdStatus = "bad password";
      Serial.println("PVX cmd: bad password");
      return;
    }
    if (doc.containsKey("inverter")) {
      _pendInverter = true;
      _valInverter = doc["inverter"].as<bool>();
    }
    if (doc.containsKey("off_grid")) {
      _pendOffGrid = true;
      _valOffGrid = doc["off_grid"].as<bool>();
    }
    if (!_pendInverter && !_pendOffGrid) {
      _lastCmdStatus = "no switch keys";
    }
  }

  // When HA owns the Solarman logger, prefer HA state over Modbus regs (contention).
  void syncSwitchesFromHa(SolarmanDeye& deye) {
    if (!haConfigured()) return;
    if (millis() - _lastHaSync < 5000) return;
    if (_pendInverter || _pendOffGrid) return;
    _lastHaSync = millis();
    bool on = false;
    // One entity per tick to keep loop responsive (MQTT + BLE).
    if (_haSyncFlip) {
      if (haSwitchGet("switch.inverter_off_grid", &on, nullptr)) {
        deye.data.off_grid = on;
        deye.data.switches_ok = true;
        deye.holdSwitchState(15000);
      }
    } else {
      if (haSwitchGet("switch.inverter", &on, nullptr)) {
        deye.data.power_on = on;
        deye.data.switches_ok = true;
        deye.holdSwitchState(15000);
      }
    }
    _haSyncFlip = !_haSyncFlip;
  }

  void applyPending(SolarmanDeye& deye) {
    if (!_pendInverter && !_pendOffGrid) return;
    bool ok = true;
    String haErr;

    if (_pendInverter) {
      _pendInverter = false;
      haErr = "";
      // HA owns Solarman logger — never fall back to Modbus when HA is configured
      // (logger lock + long TCP stalls caused WDT reboot and lost Off Grid cmds).
      bool done = false;
      if (haConfigured()) done = haSwitchSet("switch.inverter", _valInverter, &haErr);
      else done = deye.setPowerOn(_valInverter);
      if (done) {
        deye.data.power_on = _valInverter;
        deye.data.switches_ok = true;
        deye.data.lastCmdError = "";
        deye.holdSwitchState(30000);
        _lastHaSync = millis();
      } else {
        ok = false;
        if (!deye.data.lastCmdError.length())
          deye.data.lastCmdError = haErr.length() ? haErr : "power write fail";
      }
      Serial.printf("PVX cmd inverter=%d -> %s (%s)\n", (int)_valInverter, done ? "ok" : "fail",
                    haErr.length() ? haErr.c_str() : (haConfigured() ? "ha" : "modbus"));
    }
    if (_pendOffGrid) {
      _pendOffGrid = false;
      haErr = "";
      bool done = false;
      if (haConfigured()) done = haSwitchSet("switch.inverter_off_grid", _valOffGrid, &haErr);
      else done = deye.setOffGrid(_valOffGrid);
      if (done) {
        deye.data.off_grid = _valOffGrid;
        deye.data.switches_ok = true;
        deye.data.lastCmdError = "";
        deye.holdSwitchState(30000);
        _lastHaSync = millis();
      } else {
        ok = false;
        if (!deye.data.lastCmdError.length())
          deye.data.lastCmdError = haErr.length() ? haErr : "off_grid write fail";
      }
      Serial.printf("PVX cmd off_grid=%d -> %s (%s)\n", (int)_valOffGrid, done ? "ok" : "fail",
                    haErr.length() ? haErr.c_str() : (haConfigured() ? "ha" : "modbus"));
    }
    if (ok) _lastCmdStatus = "ok";
    else _lastCmdStatus = deye.data.lastCmdError.length() ? deye.data.lastCmdError : "fail";
    _lastPub = 0;
  }

  void publish(SolarmanDeye& deye, BmsManager& bms, const String& deviceId, const char* fw) {
    deye.refreshAge();
    StaticJsonDocument<1664> doc;
    doc["ts"] = (uint32_t)(millis() / 1000);
    doc["device_id"] = deviceId;
    doc["fw"] = fw;
    doc["src"] = "esp32";

    JsonObject inv = doc.createNestedObject("inverter");
    inv["ok"] = deye.data.valid;
    inv["age_ms"] = deye.data.ageMs;
    if (deye.data.lastError.length()) inv["err"] = deye.data.lastError;
    inv["state"] = deye.data.state;
    // Only publish switch values when confirmed — boot defaults (false) mislead the UI.
    inv["switches_ok"] = deye.data.switches_ok;
    if (deye.data.switches_ok) {
      inv["power_on"] = deye.data.power_on;
      inv["off_grid"] = deye.data.off_grid;
    }
    inv["pv_w"] = deye.data.pv_w;
    inv["pv1_w"] = deye.data.pv1_w;
    inv["pv2_w"] = deye.data.pv2_w;
    inv["load_w"] = deye.data.load_w;
    inv["grid_w"] = deye.data.grid_w;
    inv["external_w"] = deye.data.external_w;
    inv["ct1_w"] = deye.data.ct1_w;
    inv["ct2_w"] = deye.data.ct2_w;
    inv["ct3_w"] = deye.data.ct3_w;
    inv["battery_w"] = deye.data.battery_w;
    inv["battery_v"] = deye.data.battery_v;
    inv["battery_a"] = deye.data.battery_a;
    inv["battery_soc"] = deye.data.battery_soc;
    inv["battery_temp_c"] = deye.data.battery_temp_c;
    inv["today_pv_kwh"] = deye.data.today_pv_kwh;
    inv["today_load_kwh"] = deye.data.today_load_kwh;
    inv["today_import_kwh"] = deye.data.today_import_kwh;
    inv["today_export_kwh"] = deye.data.today_export_kwh;
    inv["today_bat_charge_kwh"] = deye.data.today_bat_charge_kwh;
    inv["today_bat_discharge_kwh"] = deye.data.today_bat_discharge_kwh;
    if (_lastCmdStatus.length()) inv["cmd"] = _lastCmdStatus;

    JsonObject basen = doc.createNestedObject("bms");
    basen["ok"] = bms.bms.valid;
    if (bms.bms.valid) {
      basen["soc"] = bms.bms.soc;
      basen["v"] = bms.bms.voltage;
      basen["a"] = bms.bms.current;
      basen["power_w"] = bms.bms.power;
      basen["temp_c"] = bms.bms.avgTemp;
      JsonArray cells = basen.createNestedArray("cells");
      for (int i = 0; i < bms.bms.cellCount && i < 16; i++) {
        cells.add(bms.bms.cellVoltages[i]);
      }
    }

    char payload[1664];
    size_t n = serializeJson(doc, payload, sizeof(payload));
    String topic = topicPrefix + "/live";
    if (!_mqtt.publish(topic.c_str(), (uint8_t*)payload, n, true)) {
      Serial.println("PVX publish fail");
      _mqtt.disconnect();
      connected = false;
    }
  }
};

PvxCloud* PvxCloud::_self = nullptr;
