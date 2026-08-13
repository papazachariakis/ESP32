#pragma once
// Mirror Classic home/<id>/status + /bms (+ /cmd sub) to public HiveMQ so
// GitHub Pages (esp32.html) can find/use the board while local HA stays on Mosquitto.
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

#ifndef HIVEMQ_PAGES_BROKER
#define HIVEMQ_PAGES_BROKER "broker.hivemq.com"
#endif
#ifndef HIVEMQ_PAGES_PORT
#define HIVEMQ_PAGES_PORT 1883
#endif
#ifndef HIVEMQ_PAGES_STATUS_MS
#define HIVEMQ_PAGES_STATUS_MS 2000
#endif
#ifndef HIVEMQ_PAGES_BMS_MS
#define HIVEMQ_PAGES_BMS_MS 1500
#endif

typedef void (*HiveMqCmdHandler)(char* topic, byte* payload, unsigned int length);

class HiveMqPagesMirror {
 public:
  bool enabled = true;
  String broker = HIVEMQ_PAGES_BROKER;
  uint16_t port = HIVEMQ_PAGES_PORT;
  bool connected = false;

  void begin(const String& deviceId, HiveMqCmdHandler cmdHandler = nullptr) {
    _deviceId = deviceId;
    _topicStatus = String("home/") + deviceId + "/status";
    _topicBms = String("home/") + deviceId + "/bms";
    _topicCmd = String("home/") + deviceId + "/cmd";
    _cmdHandler = cmdHandler;
    _self = this;
    _mqtt.setClient(_wifi);
    _mqtt.setServer(broker.c_str(), port);
    _mqtt.setBufferSize(4096);
    _mqtt.setKeepAlive(45);
    _mqtt.setSocketTimeout(3);
    _mqtt.setCallback(_staticCb);
  }

  void loop() {
    if (!enabled || WiFi.status() != WL_CONNECTED) {
      connected = false;
      return;
    }
    if (!_mqtt.connected()) {
      if (millis() - _lastConn < 5000) return;
      _lastConn = millis();
      String cid = String("esp32-hmq-") + _deviceId;
      connected = _mqtt.connect(cid.c_str());
      if (connected) {
        _mqtt.subscribe(_topicCmd.c_str());
        Serial.printf("HiveMQ Pages MQTT connected (%s)\n", broker.c_str());
        // Force a status republish soon after connect so scan finds us.
        _lastStatus = 0;
      } else {
        Serial.printf("HiveMQ Pages MQTT fail rc=%d\n", _mqtt.state());
      }
      return;
    }
    _mqtt.loop();
    connected = true;
  }

  // Publish retained status JSON (throttled). Returns true if sent this call.
  bool publishStatus(const char* json, bool force = false) {
    if (!enabled || !json) return false;
    if (!force && millis() - _lastStatus < HIVEMQ_PAGES_STATUS_MS) return false;
    if (!_ensureConnected()) return false;
    bool ok = _mqtt.publish(_topicStatus.c_str(), json, true);
    if (ok) _lastStatus = millis();
    else {
      _mqtt.disconnect();
      connected = false;
    }
    return ok;
  }

  bool publishBms(const char* json, bool force = false) {
    if (!enabled || !json) return false;
    if (!force && millis() - _lastBms < HIVEMQ_PAGES_BMS_MS) return false;
    if (!_ensureConnected()) return false;
    bool ok = _mqtt.publish(_topicBms.c_str(), json, true);
    if (ok) _lastBms = millis();
    else {
      _mqtt.disconnect();
      connected = false;
    }
    return ok;
  }

 private:
  WiFiClient _wifi;
  PubSubClient _mqtt;
  String _deviceId;
  String _topicStatus, _topicBms, _topicCmd;
  HiveMqCmdHandler _cmdHandler = nullptr;
  unsigned long _lastConn = 0;
  unsigned long _lastStatus = 0;
  unsigned long _lastBms = 0;
  static HiveMqPagesMirror* _self;

  bool _ensureConnected() {
    if (_mqtt.connected()) {
      connected = true;
      return true;
    }
    loop();
    return _mqtt.connected();
  }

  static void _staticCb(char* topic, byte* payload, unsigned int len) {
    if (_self && _self->_cmdHandler) _self->_cmdHandler(topic, payload, len);
  }
};

HiveMqPagesMirror* HiveMqPagesMirror::_self = nullptr;
