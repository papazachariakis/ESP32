/*

 * ESP32 Control Hub

 * - WiFi setup portal (WiFiManager)

 * - Web app (local)

 * - BLE client (multi-BMS)

 * - Relay / GPIO control

 * - MQTT remote access via Internet

 */



#include <WiFi.h>

#include <WiFiManager.h>

#include <esp_mac.h>
#include <esp_coexist.h>
#include <ESPmDNS.h>
#include <Preferences.h>

#include <PubSubClient.h>

#include <WebServer.h>

#include <BLEDevice.h>

#include <BLEScan.h>

#include <BLEClient.h>

#include <ArduinoJson.h>

#include "config.h"
#include "ble_radio.h"
#include "ha_mqtt_discovery.h"
#include "solarman_deye.h"
#include "pvx_cloud.h"
#if defined(ESP32_SLIM_BUILD)
#include "hivemq_pages.h"
#endif

#if defined(ESP32_SLIM_BUILD)
#include "webui_slim.h"
#else
#include "webui.h"
#endif
#include "bms_manager.h"
#include "wifi_store.h"
#include "hub_reset.h"
#include "ota.h"
#include "mqtt_auth.h"
#ifndef ESP32_SLIM_BUILD
#include "cummins_gen.h"
#include "genset_schedule.h"
#include "entes_meter.h"
#endif



WebServer server(80);

WiFiClient wifiClient;

PubSubClient mqtt(wifiClient);

Preferences prefs;

BmsManager bmsMgr;
SolarmanDeye deyeMgr;
PvxCloud pvxCloud;
#if defined(ESP32_SLIM_BUILD)
HiveMqPagesMirror hiveMqPages;
#endif
#ifndef ESP32_SLIM_BUILD
GenManager genMgr;
GenSchedule genSched;
MeterManager meterMgr;
#endif



String deviceId;

String mqttBroker = MQTT_DEFAULT_BROKER;

uint16_t mqttPort = MQTT_DEFAULT_PORT;

String mqttUser = MQTT_DEFAULT_USER;

String mqttPass = MQTT_DEFAULT_PASS;

String topicStatus, topicCmd, topicBms, topicGenset, topicMeter, topicWifi, topicBle;

String bleScanJson = "[]";



bool relayState[RELAY_COUNT] = { false };

unsigned long lastMqttPublish = 0;

unsigned long lastBleReconnect = 0;

#ifndef ESP32_SLIM_BUILD
volatile bool gModbusScanPending = false;
volatile bool gModbusLoopbackPending = false;
#endif

volatile bool gWifiScanPending = false;

volatile bool gWifiConnectPending = false;

volatile bool gForceWifiRoam = false;
volatile unsigned long gBlePreferBtUntil = 0;

volatile bool gBleScanPending = false;

volatile bool gBleConnectPending = false;

String gWifiConnectSsid;

String gWifiConnectPass;

String gBleConnectMac;

String gBleConnectName;

String gBleConnectType;

void startBleScan();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishStatus();
void publishBmsMqtt();

void pumpNetwork() {
  server.handleClient();
  if (WiFi.status() == WL_CONNECTED && mqtt.connected()) mqtt.loop();
#if defined(ESP32_SLIM_BUILD)
  hiveMqPages.loop();
#endif
  pvxPumpLive();
}

void pvxPumpLive() {
  static bool busy = false;
  if (busy) return;
  busy = true;
  pvxCloud.loop(deyeMgr, bmsMgr, deviceId, FIRMWARE_VERSION);
  busy = false;
}

bool webBodyAuthorized() {
  if (!server.hasArg("plain")) return false;
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, server.arg("plain"))) return false;
  return webJsonPasswordOk(doc);
}

void webRejectAuth() {
  server.send(403, "application/json", "{\"ok\":false,\"error\":\"bad password\"}");
}



void saveRelayStates() {

  for (int i = 0; i < RELAY_COUNT; i++) {

    prefs.putBool(("r" + String(i)).c_str(), relayState[i]);

  }

}



void setRelay(int index, bool on) {

  if (index < 0 || index >= RELAY_COUNT) return;

  relayState[index] = on;

  digitalWrite(RELAY_PINS[index], on ? HIGH : LOW);

  saveRelayStates();

}



void allRelaysOff() {

  for (int i = 0; i < RELAY_COUNT; i++) setRelay(i, false);

}



String getDeviceId() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char buf[13];
  snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

inline void startMdns() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (MDNS.begin("esp32")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS: http://esp32.local");
  } else {
    Serial.println("mDNS init failed");
  }
}



void publishGensetMqtt() {
#ifndef ESP32_SLIM_BUILD
  if (!mqtt.connected()) return;
  StaticJsonDocument<4096> doc;
  JsonObject root = doc.to<JsonObject>();
  genFillJson(root, genMgr.data, genMgr.profile);
  root["enabled"] = genMgr.enabled;
  if (!genMgr.enabled) {
    root["valid"] = false;
    root["poll_complete"] = true;
    if (!genMgr.data.lastError.length())
      root["error"] = "Modbus OFF — enable from dashboard";
  }
  char payload[4096];
  serializeJson(doc, payload);
  mqtt.publish(topicGenset.c_str(), payload);
#endif
}

#ifndef ESP32_SLIM_BUILD
void publishMeterMqtt() {
  if (!mqtt.connected() || !meterMgr.enabled) return;
  StaticJsonDocument<1536> doc;
  JsonObject root = doc.to<JsonObject>();
  meterFillJson(root, meterMgr.data);
  root["enabled"] = meterMgr.enabled;
  root["slave_id"] = meterMgr.slaveId;
  char payload[1536];
  serializeJson(doc, payload);
  mqtt.publish(topicMeter.c_str(), payload);
}
#endif

void publishBmsMqtt() {

  if (!bmsMgr.bms.valid) return;

  StaticJsonDocument<2048> doc;

  JsonObject root = doc.to<JsonObject>();

  bmsFillJson(root, bmsMgr.bms);

  char payload[2048];

  serializeJson(doc, payload);

  if (mqtt.connected()) mqtt.publish(topicBms.c_str(), payload);
#if defined(ESP32_SLIM_BUILD)
  hiveMqPages.publishBms(payload);
#endif

}



String buildStatusJson() {

  StaticJsonDocument<STATUS_JSON_CAPACITY> doc;

  doc["device_id"] = deviceId;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["board"] = BOARD_ID;
  doc["board_label"] = BOARD_LABEL;

  doc["ota_firmware_file"] = OTA_FIRMWARE_FILE;
  doc["ota_remote_path"] = OTA_REMOTE_PATH;

  doc["ip"] = WiFi.localIP().toString();

  doc["wifi_ssid"] = wifiStoreCurrentSsid(prefs);

  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;

  doc["rssi"] = WiFi.RSSI();

  JsonArray savedWifi = doc.createNestedArray("wifi_saved");
  wifiStoreAddToJson(prefs, savedWifi);

  if (otaStatusField()) doc["ota_phase"] = otaStatusField();
  if (otaErrorVisible()) doc["ota_error"] = lastOtaError();
  if (mqttOtaActive()) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%d/%d", mqttOtaReceived(), mqttOtaExpected());
    doc["ota_mqtt_rx"] = buf;
  } else if (otaDownloadTotal() > 0) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%d/%d", otaDownloadReceived(), otaDownloadTotal());
    doc["ota_mqtt_rx"] = buf;
  }

  JsonArray outputs = doc.createNestedArray("outputs");

  for (int i = 0; i < RELAY_COUNT; i++) {

    JsonObject o = outputs.add<JsonObject>();

    o["gpio"] = RELAY_PINS[i];

    o["name"] = RELAY_LABELS[i];

    o["on"] = relayState[i];

  }



  JsonObject ble = doc.createNestedObject("ble");

  ble["connected"] = bmsMgr.connected;

  ble["mac"] = bmsMgr.mac;

  ble["name"] = bmsMgr.name;

  ble["bms_type"] = bmsTypeId(bmsMgr.type);

  ble["cell_frames"] = bmsMgr.proto.cellFrames;
  ble["info_frames"] = bmsMgr.proto.infoFrames;
  ble["crc_errors"] = bmsMgr.proto.crcErrors;
  if (bmsMgr.connected && bmsMgr.bleRssi > -200) ble["rssi"] = bmsMgr.bleRssi;
  ble["tx_power_dbm"] = bleTxPowerDbm(BLE_TX_POWER_LEVEL);
  ble["data_age_ms"] = bmsMgr.bms.valid
    ? (long)(millis() - bmsMgr.bms.lastUpdate) : -1;
  if (bmsMgr.lastNotifyMs)
    ble["last_notify_ms"] = (long)(millis() - bmsMgr.lastNotifyMs);
  if (bmsMgr.lastCellMs)
    ble["last_cell_ms"] = (long)(millis() - bmsMgr.lastCellMs);



  JsonObject bmsObj = doc.createNestedObject("bms");

  bmsFillJson(bmsObj, bmsMgr.bms);



  JsonObject mq = doc.createNestedObject("mqtt");

  mq["broker"] = mqttBroker;
  mq["port"] = mqttPort;
  mq["user"] = mqttUser;
  mq["connected"] = mqtt.connected();
#if defined(ESP32_SLIM_BUILD)
  mq["hivemq"] = hiveMqPages.broker;
  mq["hivemq_connected"] = hiveMqPages.connected;
#endif

  mq["topic_status"] = topicStatus;

  mq["topic_cmd"] = topicCmd;

  mq["topic_bms"] = topicBms;

#if defined(ESP32_SLIM_BUILD)
  mq["topic_genset"] = "";
#else
  mq["topic_genset"] = topicGenset;
#endif

#ifndef ESP32_SLIM_BUILD
  mq["topic_meter"] = topicMeter;
#endif

  mq["topic_wifi"] = topicWifi;

  mq["topic_ble"] = topicBle;

#if defined(ESP32_SLIM_BUILD)
  JsonObject genset = doc.createNestedObject("genset");
  genset["enabled"] = false;
  genset["valid"] = false;
  genset["error"] = "genset removed — Classic is Basen BMS only";
  JsonObject sched = doc.createNestedObject("genset_schedule");
  sched["enabled"] = false;
#else
  JsonObject genset = doc.createNestedObject("genset");
  genFillJson(genset, genMgr.data, genMgr.profile);
  genset["enabled"] = genMgr.enabled;
  genset["slave_id"] = genMgr.slaveId;
  genset["baud"] = genMgr.baud;
  genset["probe_reg"] = genMgr.probeReg;
  genset["modbus_rx"] = MODBUS_RX_PIN;
  genset["modbus_tx"] = MODBUS_TX_PIN;
  genset["modbus_de"] = MODBUS_DE_PIN;

  JsonObject meter = doc.createNestedObject("meter");
  meterFillJson(meter, meterMgr.data);
  meter["enabled"] = meterMgr.enabled;
  meter["slave_id"] = meterMgr.slaveId;

  JsonObject sched = doc.createNestedObject("genset_schedule");
  genSched.fillJson(sched);
#endif



  String out;

  serializeJson(doc, out);

  return out;

}



void publishStatus() {

  String json = buildStatusJson();

  if (mqtt.connected()) mqtt.publish(topicStatus.c_str(), json.c_str());
#if defined(ESP32_SLIM_BUILD)
  hiveMqPages.publishStatus(json.c_str());
#endif

}

void publishWifiScan() {
  if (!mqtt.connected()) return;
  pumpNetwork();
  int found = WiFi.scanNetworks(false, true);
  StaticJsonDocument<WIFI_SCAN_JSON_CAPACITY> doc;
  JsonArray nets = doc.createNestedArray("networks");
  for (int i = 0; i < found; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    JsonObject o = nets.add<JsonObject>();
    o["ssid"] = ssid;
    o["rssi"] = WiFi.RSSI(i);
    o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  JsonArray saved = doc.createNestedArray("saved");
  wifiStoreAddToJson(prefs, saved);
  doc["connected"] = WiFi.status() == WL_CONNECTED;
  doc["current"] = wifiStoreCurrentSsid(prefs);
  doc["ip"] = WiFi.localIP().toString();
  String out;
  serializeJson(doc, out);
  mqtt.publish(topicWifi.c_str(), out.c_str(), false);
  WiFi.scanDelete();
  pumpNetwork();
}

void publishWifiResult(bool ok, const char* errorMsg = nullptr) {
  if (!mqtt.connected()) return;
  StaticJsonDocument<256> doc;
  doc["ok"] = ok;
  if (ok) {
    doc["current"] = wifiStoreCurrentSsid(prefs);
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
  } else if (errorMsg) {
    doc["error"] = errorMsg;
    doc["ssid"] = gWifiConnectSsid;
  }
  String out;
  serializeJson(doc, out);
  mqtt.publish(topicWifi.c_str(), out.c_str(), false);
}

void publishBleScan() {
  if (!mqtt.connected()) return;

  startBleScan();

  StaticJsonDocument<6144> doc;
  JsonArray devices = doc.createNestedArray("devices");
  StaticJsonDocument<4096> arrDoc;
  if (!deserializeJson(arrDoc, bleScanJson)) {
    for (JsonObject o : arrDoc.as<JsonArray>()) devices.add(o);
  }
  doc["current_mac"] = bmsMgr.mac;
  doc["current_name"] = bmsMgr.name;
  doc["connected"] = bmsMgr.connected;
  doc["bms_type"] = bmsTypeId(bmsMgr.type);
  String out;
  serializeJson(doc, out);
  mqtt.publish(topicBle.c_str(), out.c_str(), false);
}

void publishBleResult(bool ok, const char* errorMsg = nullptr) {
  if (!mqtt.connected()) return;
  StaticJsonDocument<384> doc;
  doc["ok"] = ok;
  if (ok) {
    doc["mac"] = bmsMgr.mac;
    doc["name"] = bmsMgr.name;
    doc["connected"] = bmsMgr.connected;
    doc["bms_type"] = bmsTypeId(bmsMgr.type);
  } else if (errorMsg) {
    doc["error"] = errorMsg;
  }
  String out;
  serializeJson(doc, out);
  mqtt.publish(topicBle.c_str(), out.c_str(), false);
}

BmsType bleConnectResolveType(const String& typeStr, const String& name) {
  BmsType bt = BmsType::None;
  if (typeStr != "auto" && typeStr.length()) bt = bmsTypeFromString(typeStr);
  if (bt == BmsType::None) bt = bmsDetectFromName(name);
  if (bt == BmsType::None && typeStr == "basen") bt = BmsType::Basen;
  if (bt == BmsType::None && typeStr == "jk") bt = BmsType::Jk;
  if (bt == BmsType::None) bt = BmsType::Jk;
  return bt;
}

void runBleConnectJob() {
  String mac = gBleConnectMac;
  String name = gBleConnectName;
  String typeStr = gBleConnectType;
  gBleConnectMac = "";
  gBleConnectName = "";
  gBleConnectType = "";
  if (mac.length() < 11) {
    publishBleResult(false, "invalid mac");
    return;
  }
  BmsType bt = bleConnectResolveType(typeStr, name);
  Serial.printf("MQTT BLE connect: %s [%s] %s\n", name.c_str(), bmsTypeId(bt), mac.c_str());
  // Persist intended pairing before radio attempt so auto-reconnect survives reboot
  // even when the BMS is temporarily offline / phone-held.
  prefs.putString("ble_mac", mac);
  prefs.putString("ble_name", name);
  prefs.putString("bms_type", bmsTypeId(bt));
  bmsMgr.type = bt;
  bmsMgr.name = name;
  bmsMgr.mac = mac;
  bool ok = bmsMgr.connect(bt, name, mac, prefs);
  if (ok) {
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
    gBlePreferBtUntil = millis() + 20000;
    esp_coex_preference_set(ESP_COEX_PREFER_BT);
#endif
    publishBmsMqtt();
    publishStatus();
  }
  publishBleResult(ok, ok ? nullptr : "connect_failed");
}

void runWifiConnectJob() {
  String ssid = gWifiConnectSsid;
  String pass = gWifiConnectPass;
  gWifiConnectSsid = "";
  gWifiConnectPass = "";
  if (ssid.length() == 0) {
    publishWifiResult(false, "missing ssid");
    return;
  }
  bool ok = wifiStoreConnect(prefs, ssid, pass);
  if (ok) {
    startMdns();
    mqtt.disconnect();
    mqttConnect();
    publishWifiResult(true);
    publishStatus();
  } else {
    publishWifiResult(false, "connect failed");
    wifiStoreTryConnect(prefs);
    if (WiFi.status() == WL_CONNECTED) startMdns();
    publishStatus();
  }
}



void mqttCallback(char* topic, byte* payload, unsigned int length) {

  if (length > 14 && memcmp(payload, "{\"ota_chunk\":", 13) == 0) {
    mqttOtaFeedChunkJson((const char*)payload, length);
    return;
  }

  StaticJsonDocument<1024> doc;

  if (deserializeJson(doc, payload, length)) return;

  if (doc.containsKey("relay")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: relay rejected (bad password)");
      return;
    }
    if (doc["relay"] == "all") {
      allRelaysOff();
    } else {
      int idx = doc["relay"].as<int>();
      bool on = doc["on"] | false;
      setRelay(idx, on);
    }
    publishStatus();
  }

#ifndef ESP32_SLIM_BUILD
  if (doc.containsKey("genset")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: genset rejected (bad password)");
      return;
    }
    const char* action = doc["genset"];
    bool ok = action && genMgr.runGensetCmd(action);
    if (action && strncmp(action, "mode_", 5) == 0) {
      genMgr.save(prefs);  // persist ESP remote gate even if STOP part failed
    }
    if (!ok) {
      Serial.printf("MQTT: genset cmd '%s' failed: %s\n",
                    action ? action : "?", genMgr.data.lastError.c_str());
    } else {
      Serial.printf("MQTT: genset cmd '%s' ok\n", action);
    }
    genMgr.pollOnce();
    publishGensetMqtt();
    publishStatus();
  }

  if (doc.containsKey("genset_schedule")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: genset_schedule rejected (bad password)");
      return;
    }
    genSched.applyJson(doc["genset_schedule"].as<JsonObject>());
    genSched.save(prefs);
    Serial.println("MQTT: genset_schedule updated");
    publishStatus();
  }

  if (doc.containsKey("genset_delay")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: genset_delay rejected (bad password)");
      return;
    }
    JsonObject gd = doc["genset_delay"];
    float startSec = gd.containsKey("start_sec") ? gd["start_sec"].as<float>() : -1;
    float stopSec = gd.containsKey("stop_sec") ? gd["stop_sec"].as<float>() : -1;
    if (startSec < 0 && gd.containsKey("start_pre_sec"))
      startSec = gd["start_pre_sec"].as<float>();
    if (stopSec < 0 && gd.containsKey("stop_pre_sec"))
      stopSec = gd["stop_pre_sec"].as<float>();
    bool ok = genMgr.setDelaySeconds(startSec, stopSec);
    if (!ok) {
      Serial.printf("MQTT: genset_delay failed: %s\n", genMgr.data.lastError.c_str());
    } else {
      Serial.println("MQTT: genset_delay updated");
    }
    genMgr.pollOnce();
    publishGensetMqtt();
    publishStatus();
  }

  if (doc.containsKey("modbus_cfg")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: modbus_cfg rejected (bad password)");
      return;
    }
    JsonObject cfg = doc["modbus_cfg"];
    if (cfg.containsKey("baud")) {
      uint32_t b = cfg["baud"].as<uint32_t>();
      if (!modbusBaudValid(b)) {
        genMgr.data.lastScan = "CFG rejected: invalid baud " + String(b);
        publishGensetMqtt();
        publishStatus();
        return;
      }
    }
    if (cfg.containsKey("enabled")) genMgr.enabled = cfg["enabled"].as<bool>();
    if (cfg.containsKey("slave_id")) genMgr.slaveId = (uint8_t)(cfg["slave_id"].as<int>());
    if (cfg.containsKey("baud")) genMgr.baud = cfg["baud"].as<uint32_t>();
    if (cfg.containsKey("probe_reg")) genMgr.probeReg = (uint16_t)(cfg["probe_reg"].as<int>());
    if (cfg.containsKey("profile")) {
      const char* p = cfg["profile"];
      if (p && (strcmp(p, "entes") == 0 || strcmp(p, "ENTES_MPR46S") == 0))
        genMgr.profile = MODBUS_PROFILE_ENTES;
      else if (p && (strcmp(p, "ps0600") == 0 || strcmp(p, "PS0600") == 0))
        genMgr.profile = MODBUS_PROFILE_PS0600;
    }
    genMgr.save(prefs);
    genMgr.applyBaud();
    if (cfg.containsKey("meter_enabled")) meterMgr.enabled = cfg["meter_enabled"].as<bool>();
    if (cfg.containsKey("meter_slave_id")) meterMgr.slaveId = (uint8_t)(cfg["meter_slave_id"].as<int>());
    meterMgr.save(prefs);
    genMgr.data.lastScan = "CFG OK baud=" + String(genMgr.baud) + " slave=" + String(genMgr.slaveId);
    Serial.printf("MQTT: modbus cfg baud=%u id=%u\n", genMgr.baud, genMgr.slaveId);
    if (genMgr.enabled) genMgr.pollOnce();
    if (meterMgr.enabled && genMgr.profile == MODBUS_PROFILE_PS0600) meterMgr.poll();
    publishGensetMqtt();
    publishMeterMqtt();
    publishStatus();
  }

  if (doc.containsKey("modbus_scan")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: modbus_scan rejected (bad password)");
      return;
    }
    Serial.println("MQTT: modbus scan requested");
    gModbusScanPending = true;
  }

  if (doc.containsKey("modbus_loopback")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: modbus_loopback rejected (bad password)");
      return;
    }
    Serial.println("MQTT: modbus loopback requested");
    gModbusLoopbackPending = true;
  }
#endif

  if (doc.containsKey("reboot")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: reboot rejected (bad password)");
      return;
    }
    Serial.println("MQTT: reboot requested");
    publishStatus();
    delay(300);
    ESP.restart();
  }

  if (doc.containsKey("ota_abort")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: ota_abort rejected (bad password)");
      return;
    }
    Serial.println("MQTT: OTA abort / reset");
    otaAbortRemote();
    publishStatus();
  }

  if (doc.containsKey("ota")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: ota rejected (bad password)");
      return;
    }
    Serial.println("MQTT: remote OTA queued");
    requestRemoteOta();
  }

  if (doc.containsKey("ota_http") && doc.containsKey("url")) {
    if (!mqttDocAuthorized(doc)) return;
    const char* url = doc["url"];
    if (url) {
      Serial.println("MQTT: HTTP OTA queued");
      requestHttpOta(url);
    }
  }

  if (doc.containsKey("ota_mqtt")) {
    if (!mqttDocAuthorized(doc)) return;
    int size = doc["size"] | 0;
    Serial.printf("MQTT: chunk OTA begin %d\n", size);
    mqttOtaBegin(size);
  }

  if (doc.containsKey("ota_end")) {
    if (!mqttDocAuthorized(doc)) return;
    Serial.println("MQTT: chunk OTA end");
    mqttOtaFinish();
  }

  if (doc.containsKey("wifi_scan")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: wifi_scan rejected (bad password)");
      return;
    }
    Serial.println("MQTT: wifi scan requested");
    gWifiScanPending = true;
  }

  if (doc.containsKey("wifi_connect")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: wifi_connect rejected (bad password)");
      return;
    }
    JsonObject w = doc["wifi_connect"];
    const char* ssid = w["ssid"];
    if (ssid && ssid[0]) {
      gWifiConnectSsid = ssid;
      gWifiConnectPass = w["pass"] | "";
      gWifiConnectPending = true;
      Serial.printf("MQTT: wifi connect %s\n", ssid);
    }
  }

  if (doc.containsKey("ble_scan")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: ble_scan rejected (bad password)");
      return;
    }
    Serial.println("MQTT: ble scan requested");
    gBleScanPending = true;
  }

  if (doc.containsKey("ble_connect")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: ble_connect rejected (bad password)");
      return;
    }
    JsonObject b = doc["ble_connect"];
    const char* mac = b["mac"];
    if (mac && mac[0]) {
      gBleConnectMac = mac;
      gBleConnectName = b["name"] | "";
      gBleConnectType = b["type"] | "auto";
      gBleConnectPending = true;
      Serial.printf("MQTT: ble connect %s\n", mac);
    }
  }

  if (doc.containsKey("ble_disconnect")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: ble_disconnect rejected (bad password)");
      return;
    }
    Serial.println("MQTT: ble disconnect requested");
    bmsMgr.disconnect(prefs);
    publishBmsMqtt();
    publishStatus();
    publishBleResult(true);
  }

  if (doc.containsKey("factory_reset")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: factory_reset rejected (bad password)");
      return;
    }
    Serial.println("MQTT: factory reset requested");
    publishStatus();
    delay(300);
    performFactoryReset(prefs);
  }

  if (doc.containsKey("wifi_clear")) {
    if (!mqttDocAuthorized(doc)) {
      Serial.println("MQTT: wifi_clear rejected (bad password)");
      return;
    }
    Serial.println("MQTT: wifi clear requested");
    publishStatus();
    delay(300);
    performWifiClear(prefs);
  }

}



bool mqttConnect() {

  if (mqttBroker.isEmpty()) return false;

  mqtt.setServer(mqttBroker.c_str(), mqttPort);

  mqtt.setCallback(mqttCallback);

  mqtt.setBufferSize(10240);
#if defined(ESP32_SLIM_BUILD)
  // Short socket timeout — default ~15s blocks Classic loop / web server.
  mqtt.setSocketTimeout(3);
  mqtt.setKeepAlive(30);
#endif

  String clientId = "esp32-" + deviceId;

  bool ok = false;
  if (mqttUser.length()) {
    ok = mqtt.connect(clientId.c_str(), mqttUser.c_str(), mqttPass.c_str());
  } else {
    ok = mqtt.connect(clientId.c_str());
  }

  if (ok) {

    mqtt.subscribe(topicCmd.c_str());

#if defined(ESP32_SLIM_BUILD)
    // Classic Basen only — never let S3 overwrite the same unique_id discovery.
    publishHaBmsDiscovery(mqtt, deviceId, topicBms, FIRMWARE_VERSION);
#endif
    publishStatus();
    if (bmsMgr.bms.valid) publishBmsMqtt();

    Serial.println("MQTT connected");

    return true;

  }

  Serial.print("MQTT failed: ");

  Serial.println(mqtt.state());

  return false;

}



void loadSettings() {

  prefs.begin("esp32ctl", false);

  mqttBroker = prefs.getString("mqtt_broker", MQTT_DEFAULT_BROKER);

  mqttPort = prefs.getUInt("mqtt_port", MQTT_DEFAULT_PORT);

  mqttUser = prefs.getString("mqtt_user", MQTT_DEFAULT_USER);

  mqttPass = prefs.getString("mqtt_pass", MQTT_DEFAULT_PASS);

  bmsMgr.mac = prefs.getString("ble_mac", "");

  bmsMgr.name = prefs.getString("ble_name", "");

  bmsMgr.type = bmsTypeFromString(prefs.getString("bms_type", ""));
#if !defined(ESP32_SLIM_BUILD)
  // 3.0.70: drop sticky JK pairing that blocked HTTP when the BMS was offline.
  if (!prefs.getBool("ble_fix_370", false)) {
    prefs.putBool("ble_fix_370", true);
    prefs.remove("ble_mac");
    prefs.remove("ble_name");
    prefs.remove("bms_type");
    bmsMgr.mac = "";
    bmsMgr.name = "";
    bmsMgr.type = BmsType::None;
    Serial.println("S3: cleared sticky BLE prefs (HTTP starve fix)");
  }
#endif
#if defined(BMS_DEFAULT_MAC)
  if (!bmsMgr.mac.length()) {
    bmsMgr.mac = BMS_DEFAULT_MAC;
    bmsMgr.name = BMS_DEFAULT_NAME;
    bmsMgr.type = bmsTypeFromString(BMS_DEFAULT_TYPE);
    prefs.putString("ble_mac", bmsMgr.mac);
    prefs.putString("ble_name", bmsMgr.name);
    prefs.putString("bms_type", BMS_DEFAULT_TYPE);
    Serial.printf("BMS default seeded: %s [%s] %s\n",
                  bmsMgr.name.c_str(), BMS_DEFAULT_TYPE, bmsMgr.mac.c_str());
  }
#endif
  // Only infer type from name when prefs have no type — never override an
  // explicit JK pairing (S3 JK uses Espressif MAC + hex serial name).
  if (bmsMgr.name.length() && bmsMgr.type == BmsType::None) {
    BmsType detected = bmsDetectFromName(bmsMgr.name);
    if (detected != BmsType::None) {
      Serial.printf("BMS type inferred: %s (%s)\n", bmsTypeId(detected), bmsMgr.name.c_str());
      bmsMgr.type = detected;
      prefs.putString("bms_type", bmsTypeId(detected));
    }
  }
  // Classic only: force known Basen default identity even if prefs were saved
  // as JK by the old TP_BSTBD -> "_BD" mis-detect bug.
#if defined(ESP32_SLIM_BUILD) && defined(BMS_DEFAULT_MAC)
  {
    String defMac = BMS_DEFAULT_MAC;
    defMac.toUpperCase();
    String curMac = bmsMgr.mac;
    curMac.toUpperCase();
    if (curMac == defMac) {
      bmsMgr.name = BMS_DEFAULT_NAME;
      bmsMgr.type = BmsType::Basen;
      prefs.putString("ble_name", bmsMgr.name);
      prefs.putString("bms_type", "basen");
    }
  }
  // Classic only: hex-serial + Espressif MAC was a false ESP32 advertise, not JK.
  if (bmsMgr.type == BmsType::Jk && bmsNameLooksLikeHexSerial(bmsMgr.name)) {
    Serial.printf("BMS hex serial %s -> basen (was jk)\n", bmsMgr.name.c_str());
    bmsMgr.type = BmsType::Basen;
    prefs.putString("bms_type", "basen");
  }
  if (bmsMgr.mac.length() && bmsMacLooksEspressif(bmsMgr.mac) && bmsNameLooksLikeHexSerial(bmsMgr.name)) {
    Serial.println("BMS saved MAC looks like ESP32 module — clearing stale BLE prefs");
    bmsMgr.mac = "";
    bmsMgr.name = "";
    bmsMgr.type = BmsType::None;
    prefs.remove("ble_mac");
    prefs.remove("ble_name");
    prefs.remove("bms_type");
  }
  if (!bmsMgr.mac.length()) {
    bmsMgr.mac = BMS_DEFAULT_MAC;
    bmsMgr.name = BMS_DEFAULT_NAME;
    bmsMgr.type = BmsType::Basen;
    prefs.putString("ble_mac", bmsMgr.mac);
    prefs.putString("ble_name", bmsMgr.name);
    prefs.putString("bms_type", "basen");
    Serial.printf("BMS default re-seeded: %s %s\n", bmsMgr.name.c_str(), bmsMgr.mac.c_str());
  }
#endif
  if (bmsMgr.type == BmsType::None && bmsMgr.name.length())
    bmsMgr.type = bmsDetectFromName(bmsMgr.name);
#if defined(ESP32_SLIM_BUILD)
  if (bmsMgr.type == BmsType::None && bmsMgr.mac.length()) bmsMgr.type = BmsType::Basen;
#else
  if (bmsMgr.type == BmsType::None && bmsMgr.mac.length()) bmsMgr.type = BmsType::Jk;
#endif

#ifndef ESP32_SLIM_BUILD
  genMgr.load(prefs);
  genMgr.pollIntervalMs = MODBUS_POLL_INTERVAL_MS;
  genSched.load(prefs);
  genEvents().begin(prefs);
  meterMgr.load(prefs);
#endif

  for (int i = 0; i < RELAY_COUNT; i++) {

    relayState[i] = prefs.getBool(("r" + String(i)).c_str(), false);

    digitalWrite(RELAY_PINS[i], relayState[i] ? HIGH : LOW);

  }

}



void setupWiFi() {

  WiFi.mode(WIFI_STA);

  wifiStoreSeedDefaults(prefs);
  wifiStoreEnsureSeeds(prefs);

  // After OTE was added as primary: prefer strongest saved AP (not sticky last SSID).
  if (!prefs.getBool("wifi_best_374", false)) {
    prefs.putBool("wifi_best_374", true);
    if (wifiStoreTryBestSaved(prefs, 0) || wifiStoreTryConnect(prefs)) {
      wifiApplyStaTuning();
      Serial.print("WiFi OK (best saved / OTE migrate): ");
      Serial.println(WiFi.localIP());
      startMdns();
      return;
    }
  }

  bool forcePortal = prefs.getBool(WIFI_FORCE_PORTAL_KEY, false);
  if (forcePortal) prefs.putBool(WIFI_FORCE_PORTAL_KEY, false);

  if (!forcePortal && wifiStoreTryConnect(prefs)) {
    wifiApplyStaTuning();
    Serial.print("WiFi OK (saved list): ");
    Serial.println(WiFi.localIP());
    startMdns();
    return;
  }

  WiFiManager wm;

  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_SEC);

  wm.setConnectTimeout(30);

  wm.setSaveConfigCallback([&wm]() {
    wifiStoreUpsert(prefs, wm.getWiFiSSID(), wm.getWiFiPass());
  });

  bool ok = forcePortal ? wm.startConfigPortal(WIFI_PORTAL_NAME) : wm.autoConnect(WIFI_PORTAL_NAME);

  if (!ok) {

    Serial.println("WiFi failed, restarting...");

    delay(2000);

    ESP.restart();

  }

  wifiStoreUpsert(prefs, wm.getWiFiSSID(), wm.getWiFiPass(false));
  wifiStoreRememberSsid(prefs, wm.getWiFiSSID());
  wifiApplyStaTuning();

  Serial.print("WiFi OK: ");

  Serial.println(WiFi.localIP());
  startMdns();

}



void startBleScan() {
  pumpNetwork();
  configureBleRadio();
  WiFi.setSleep(false);
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
  esp_coex_preference_set(ESP_COEX_PREFER_BT);
#endif
  if (bmsMgr.client && bmsMgr.client->isConnected()) {
    Serial.println("BLE scan: disconnecting BMS link so it can advertise");
    bmsMgr.client->disconnect();
    delay(250);
  }

  BLEScan* scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(BLE_SCAN_INTERVAL_MS);
  scan->setWindow(BLE_SCAN_WINDOW_MS);

  scan->clearResults();

  Serial.printf("BLE scan %ds (active, interval=%ums window=%ums)\n",
                BLE_SCAN_SECONDS, BLE_SCAN_INTERVAL_MS, BLE_SCAN_WINDOW_MS);
  BLEScanResults* results = scan->start(BLE_SCAN_SECONDS, false);



  StaticJsonDocument<6144> doc;

  JsonArray arr = doc.to<JsonArray>();

  int count = results ? results->getCount() : 0;
  int bmsCount = 0;

  for (int i = 0; i < count; i++) {

    BLEAdvertisedDevice d = results->getDevice(i);

    String name = d.getName().c_str();

    BmsType bt = bmsDetectType(name, d);

    if (bt == BmsType::None) continue;
    bmsCount++;

    JsonObject o = arr.add<JsonObject>();

    o["name"] = name;

    o["mac"] = d.getAddress().toString().c_str();

    o["rssi"] = d.getRSSI();

    o["bms_type"] = bmsTypeId(bt);

    o["bms_label"] = bmsTypeLabel(bt);

    o["jk"] = bt == BmsType::Jk;
    o["basen"] = bt == BmsType::Basen;

  }

  bleScanJson = "";

  serializeJson(arr, bleScanJson);

  scan->clearResults();
  pumpNetwork();
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
  // Restore WiFi priority after temporary BT bias during scan.
  esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
  WiFi.setSleep(false);
#endif
  Serial.printf("BLE scan done: %d devices, %d BMS\n", count, bmsCount);
}



void handleRoot() {
  server.send_P(200, "text/html", WEB_UI);
}

void handleStatus() {

  server.send(200, "application/json", buildStatusJson());

}



void handleRelay() {

  if (!server.hasArg("plain")) {

    server.send(400, "application/json", "{\"error\":\"no body\"}");

    return;

  }

  StaticJsonDocument<128> doc;

  if (deserializeJson(doc, server.arg("plain"))) {

    server.send(400, "application/json", "{\"error\":\"bad json\"}");

    return;

  }

  if (!webJsonPasswordOk(doc)) { webRejectAuth(); return; }

  setRelay(doc["index"] | 0, doc["on"] | false);

  publishStatus();

  server.send(200, "application/json", "{\"ok\":true}");

}



void handleAllOff() {

  if (!webBodyAuthorized()) { webRejectAuth(); return; }

  allRelaysOff();

  publishStatus();

  server.send(200, "application/json", "{\"ok\":true}");

}



void handleBleScan() {

  if (!webBodyAuthorized()) { webRejectAuth(); return; }

  startBleScan();

  server.send(200, "application/json", "{\"devices\":" + bleScanJson + "}");

}



void handleBleConnect() {

  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"no body\"}");
    return;
  }

  StaticJsonDocument<256> doc;

  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"bad json\"}");
    return;
  }

  if (!webJsonPasswordOk(doc)) { webRejectAuth(); return; }

  String mac = doc["mac"] | "";

  String name = doc["name"] | "";

  String typeStr = doc["type"] | "auto";

  if (mac.length() < 11) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid mac\"}");
    return;
  }

  BmsType bt = bleConnectResolveType(typeStr, name);
  if (bt == BmsType::None) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"unknown_bms_type\"}");
    return;
  }

  Serial.printf("BLE connect request: %s [%s] %s\n", name.c_str(), bmsTypeId(bt), mac.c_str());

  bool ok = bmsMgr.connect(bt, name, mac, prefs);

  if (ok) {
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
    gBlePreferBtUntil = millis() + 20000;
    esp_coex_preference_set(ESP_COEX_PREFER_BT);
#endif
    publishBmsMqtt();
    publishStatus();
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }

  server.send(500, "application/json", "{\"ok\":false,\"error\":\"connect_failed\"}");

}



#ifndef ESP32_SLIM_BUILD
void handleGenset() {

  StaticJsonDocument<2560> doc;

  JsonObject o = doc.to<JsonObject>();

  genFillJson(o, genMgr.data, genMgr.profile);

  o["enabled"] = genMgr.enabled;

  o["slave_id"] = genMgr.slaveId;

  o["baud"] = genMgr.baud;

  o["probe_reg"] = genMgr.probeReg;

  String out;

  serializeJson(doc, out);

  server.send(200, "application/json", out);

}
#endif




#ifndef ESP32_SLIM_BUILD
void handleGensetCmd() {

  if (!server.hasArg("plain")) {

    server.send(400, "application/json", "{\"ok\":false}");

    return;

  }

  StaticJsonDocument<128> doc;

  if (deserializeJson(doc, server.arg("plain"))) {

    server.send(400, "application/json", "{\"ok\":false}");

    return;

  }

  if (!webJsonPasswordOk(doc)) { webRejectAuth(); return; }

  const char* action = doc["action"];

  if (!action || !genMgr.runGensetCmd(action)) {

    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_action\"}");

    return;

  }

  if (strncmp(action, "mode_", 5) == 0) genMgr.save(prefs);

  genMgr.pollOnce();

  publishGensetMqtt();

  publishStatus();

  server.send(200, "application/json", "{\"ok\":true}");

}
#endif




#ifndef ESP32_SLIM_BUILD
void handleGensetScheduleGet() {
  StaticJsonDocument<1024> doc;
  JsonObject o = doc.to<JsonObject>();
  genSched.fillJson(o);
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}
#endif




#ifndef ESP32_SLIM_BUILD
void handleGensetSchedulePost() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  if (!webJsonPasswordOk(doc)) { webRejectAuth(); return; }
  JsonObject cfg = doc.containsKey("genset_schedule")
    ? doc["genset_schedule"].as<JsonObject>() : doc.as<JsonObject>();
  genSched.applyJson(cfg);
  genSched.save(prefs);
  publishStatus();
  server.send(200, "application/json", "{\"ok\":true}");
}
#endif




#ifndef ESP32_SLIM_BUILD
void handleModbusSave() {

  if (!server.hasArg("plain")) {

    server.send(400, "application/json", "{\"ok\":false}");

    return;

  }

  StaticJsonDocument<256> doc;

  if (deserializeJson(doc, server.arg("plain"))) {

    server.send(400, "application/json", "{\"ok\":false}");

    return;

  }

  if (!webJsonPasswordOk(doc)) { webRejectAuth(); return; }

  genMgr.enabled = doc["enabled"] | true;

  genMgr.slaveId = (uint8_t)(doc["slave_id"] | 1);

  if (doc.containsKey("probe_reg")) genMgr.probeReg = (uint16_t)(doc["probe_reg"].as<int>());

  genMgr.baud = doc["baud"] | 9600;
  if (!modbusBaudValid(genMgr.baud)) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid baud\"}");
    return;
  }

  if (doc.containsKey("profile")) {
    const char* p = doc["profile"];
    if (p && (strcmp(p, "entes") == 0 || strcmp(p, "ENTES_MPR46S") == 0))
      genMgr.profile = MODBUS_PROFILE_ENTES;
    else if (p && (strcmp(p, "ps0600") == 0 || strcmp(p, "PS0600") == 0))
      genMgr.profile = MODBUS_PROFILE_PS0600;
    else
      genMgr.profile = (uint8_t)(doc["profile"] | genMgr.profile);
  }

  genMgr.save(prefs);

  genMgr.applyBaud();

#ifndef ESP32_SLIM_BUILD
  if (doc.containsKey("meter_enabled")) meterMgr.enabled = doc["meter_enabled"] | true;
  if (doc.containsKey("meter_slave_id")) meterMgr.slaveId = (uint8_t)(doc["meter_slave_id"] | MODBUS_DEFAULT_METER_SLAVE_ID);
  meterMgr.save(prefs);
#endif

  if (genMgr.enabled) genMgr.pollOnce();

  server.send(200, "application/json", "{\"ok\":true}");

}
#endif




#ifndef ESP32_SLIM_BUILD
void handleModbusScan() {
  if (!webBodyAuthorized()) { webRejectAuth(); return; }
  bool found = genMgr.scanBus(1, 8);
  if (found) {
    genMgr.enabled = true;
    genMgr.save(prefs);
    genMgr.pollOnce();
  }
  StaticJsonDocument<192> doc;
  doc["ok"] = found;
  doc["result"] = genMgr.data.lastScan;
  doc["baud"] = genMgr.baud;
  doc["slave_id"] = genMgr.slaveId;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}
#endif




#ifndef ESP32_SLIM_BUILD
void handleModbusLoopback() {
  if (!webBodyAuthorized()) { webRejectAuth(); return; }
  String r = genMgr.loopbackTest();
  genMgr.applyBaud();
  StaticJsonDocument<160> doc;
  doc["ok"] = r.startsWith("LOOPBACK OK");
  doc["result"] = r;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}
#endif




void handleBleDisconnect() {

  if (!webBodyAuthorized()) { webRejectAuth(); return; }

  bmsMgr.disconnect(prefs);

  server.send(200, "application/json", "{\"ok\":true}");

}



void handleWifiReset() {

  prefs.putBool(WIFI_FORCE_PORTAL_KEY, true);

  server.send(200, "application/json", "{\"ok\":true}");

  delay(500);

  WiFiManager wm;

  wm.resetSettings();

  ESP.restart();

}

void handleWifiClear() {
  StaticJsonDocument<128> doc;
  if (server.hasArg("plain")) deserializeJson(doc, server.arg("plain"));
  const char* pw = doc["password"] | doc["wifi_clear"] | "";
  if (!remoteOtaPasswordOk(pw)) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"bad password\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true,\"action\":\"wifi_clear\"}");
  delay(300);
  performWifiClear(prefs);
}

void handleFactoryReset() {
  StaticJsonDocument<128> doc;
  if (server.hasArg("plain")) deserializeJson(doc, server.arg("plain"));
  const char* pw = doc["password"] | doc["factory_reset"] | "";
  if (!remoteOtaPasswordOk(pw)) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"bad password\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true,\"action\":\"factory_reset\"}");
  delay(300);
  performFactoryReset(prefs);
}



void handleMqttSave() {

  StaticJsonDocument<384> doc;

  deserializeJson(doc, server.arg("plain"));

  mqttBroker = doc["broker"] | MQTT_DEFAULT_BROKER;

  mqttPort = doc["port"] | MQTT_DEFAULT_PORT;

  if (doc.containsKey("user") || doc.containsKey("username")) {
    mqttUser = doc["user"] | doc["username"] | "";
  }
  if (doc.containsKey("pass") || doc.containsKey("password")) {
    // Do not treat DEVICE cmd password field as MQTT pass when only broker is sent.
    const char* p = doc["pass"] | "";
    if (p[0]) mqttPass = p;
    else if (doc["password"].is<const char*>() && doc.containsKey("user")) {
      mqttPass = doc["password"] | "";
    }
  }
  if (doc.containsKey("mqtt_pass")) mqttPass = doc["mqtt_pass"] | mqttPass;

  prefs.putString("mqtt_broker", mqttBroker);

  prefs.putUInt("mqtt_port", mqttPort);

  prefs.putString("mqtt_user", mqttUser);

  prefs.putString("mqtt_pass", mqttPass);

  mqtt.disconnect();

  mqttConnect();

  server.send(200, "application/json", "{\"ok\":true}");

}



void setupRoutes() {

  server.on("/", HTTP_GET, handleRoot);
  server.on("/app", HTTP_GET, handleRoot);

  server.on("/api/status", HTTP_GET, handleStatus);

  server.on("/api/relay", HTTP_POST, handleRelay);

  server.on("/api/relay/alloff", HTTP_POST, handleAllOff);

  server.on("/api/ble/scan", HTTP_POST, handleBleScan);

  server.on("/api/ble/connect", HTTP_POST, handleBleConnect);

  server.on("/api/ble/disconnect", HTTP_POST, handleBleDisconnect);

  server.on("/api/wifi/reset", HTTP_POST, handleWifiReset);
  server.on("/api/wifi/clear", HTTP_POST, handleWifiClear);
  server.on("/api/factory/reset", HTTP_POST, handleFactoryReset);

  server.on("/api/mqtt", HTTP_POST, handleMqttSave);
#ifndef ESP32_SLIM_BUILD
  server.on("/api/genset", HTTP_GET, handleGenset);
  server.on("/api/genset/cmd", HTTP_POST, handleGensetCmd);
  server.on("/api/genset/schedule", HTTP_GET, handleGensetScheduleGet);
  server.on("/api/genset/schedule", HTTP_POST, handleGensetSchedulePost);
  server.on("/api/modbus", HTTP_POST, handleModbusSave);
  server.on("/api/modbus/scan", HTTP_POST, handleModbusScan);
  server.on("/api/modbus/loopback", HTTP_POST, handleModbusLoopback);
#endif
  registerOtaRoutes(server);

  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });

  server.begin();

}



void setup() {

  Serial.begin(115200);

  delay(500);



  for (int i = 0; i < RELAY_COUNT; i++) {

    pinMode(RELAY_PINS[i], OUTPUT);

    digitalWrite(RELAY_PINS[i], LOW);

  }



  loadSettings();

  deviceId = getDeviceId();

  topicStatus = "home/" + deviceId + "/status";

  topicCmd = "home/" + deviceId + "/cmd";

  topicBms = "home/" + deviceId + "/bms";

#ifndef ESP32_SLIM_BUILD
  topicGenset = "home/" + deviceId + "/genset";
#endif

#ifndef ESP32_SLIM_BUILD
  topicMeter = "home/" + deviceId + "/meter";
#endif

  topicWifi = "home/" + deviceId + "/wifi";
  topicBle = "home/" + deviceId + "/ble";

  deyeMgr.begin();
  pvxCloud.begin(deviceId);
#if defined(ESP32_SLIM_BUILD)
  hiveMqPages.begin(deviceId, mqttCallback);
#endif

  BLEDevice::init("ESP32-Control");
  configureBleRadio();

  setupWiFi();

#ifndef ESP32_SLIM_BUILD
  genMgr.begin();
  meterMgr.attach(&Serial2, MODBUS_DE_PIN);
  if (genMgr.enabled) genMgr.pollOnce();
#endif



  setupRoutes();
  setupArduinoOta();
#ifndef ESP32_SLIM_BUILD
  genSched.begin();
#endif
  otaPrepHook() = []() {
    if (bmsMgr.client && bmsMgr.client->isConnected()) {
      bmsMgr.client->disconnect();
      bmsMgr.connected = false;
    }
  };
  otaStatusHook() = []() { publishStatus(); };
  otaPumpHook() = pumpNetwork;
  modbusSetPump(pumpNetwork);
  mqttConnect();



  if (bmsMgr.mac.length() > 0) {
    Serial.println("BMS saved, auto-reconnect from loop: " + bmsMgr.mac);
    // Trigger the first reconnect attempt ~4s after boot instead of waiting
    // the full BLE_RECONNECT_MS interval.
    unsigned long headStart = (BLE_RECONNECT_MS > 4000) ? (BLE_RECONNECT_MS - 4000) : 0;
    lastBleReconnect = millis() - headStart;
  }



  Serial.println("ESP32 Control Hub ready");
  Serial.printf("Board: %s (%s)\n", BOARD_LABEL, BOARD_ID);
  Serial.printf("Device ID: %s\n", deviceId.c_str());
#ifndef ESP32_SLIM_BUILD
  Serial.printf("Modbus RX=%d TX=%d DE=%d\n", MODBUS_RX_PIN, MODBUS_TX_PIN, MODBUS_DE_PIN);
#else
  Serial.println("Classic: Basen BMS only (genset/modbus removed)");
#endif

  Serial.println("Open http://" + WiFi.localIP().toString());

}



void loop() {
  pumpNetwork();

  if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
    mqtt.loop();
    if (mqttOtaActive()) {
      for (int i = 0; i < 16; i++) mqtt.loop();
      if (millis() - lastMqttPublish > 400) {
        lastMqttPublish = millis();
        publishStatus();
      }
      return;
    }
  }

  if (remoteOtaPending() && !otaInProgress()) {
    remoteOtaPending() = false;
    performRemoteOta();
  }

  if (httpOtaPending() && !otaInProgress()) {
    httpOtaPending() = false;
    performPendingHttpOta();
  }

  if (otaInProgress() && !mqttOtaActive()) return;

  static bool blePausedForWifi = false;
  static unsigned long wifiDownSince = 0;

  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastWifiTry = 0;
    static uint8_t wifiFailStreak = 0;
    static unsigned long wifiKickAt = 0;

    if (!wifiDownSince) wifiDownSince = millis();

#if WIFI_DOWN_DEBOUNCE_MS > 0
    // Ignore brief WiFi status glitches — kicking STA made Classic drop in a loop.
    if (millis() - wifiDownSince < WIFI_DOWN_DEBOUNCE_MS) {
      // still pause BLE quickly below after debounce window via separate check
    } else
#endif
    // Free BLE radio quickly so STA can recover (Classic: ~0.5s after debounce, S3: ~8s).
    if (!blePausedForWifi && (millis() - wifiDownSince > BLE_PAUSE_WIFI_DOWN_MS + WIFI_DOWN_DEBOUNCE_MS)) {
      if (bmsMgr.connected || (bmsMgr.client && bmsMgr.client->isConnected())) {
        Serial.println("WiFi down — pausing BLE for radio recovery");
        bmsMgr.dropLink();
      }
      blePausedForWifi = true;
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
      esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
#endif
    }

#if WIFI_RECOVERY_NONBLOCK
    // Count a failed kick only after grace — begin() is async on Classic.
    if (wifiKickAt && (millis() - wifiKickAt > WIFI_KICK_GRACE_MS)) {
      wifiKickAt = 0;
      if (WiFi.status() != WL_CONNECTED) {
        wifiFailStreak++;
        Serial.printf("WiFi kick grace expired (%u/%u)\n",
                      wifiFailStreak, WIFI_RECONNECT_FAIL_RESTART);
        if (wifiFailStreak >= WIFI_RECONNECT_FAIL_RESTART) {
          Serial.println("WiFi reconnect exhausted, restarting...");
          delay(500);
          ESP.restart();
        }
      }
    }
#endif

    if (millis() - lastWifiTry > WIFI_RECONNECT_INTERVAL_MS) {
#if WIFI_DOWN_DEBOUNCE_MS > 0
      if (millis() - wifiDownSince < WIFI_DOWN_DEBOUNCE_MS) {
        // wait out glitch before kicking radio
      } else
#endif
      {
      lastWifiTry = millis();
      wifiApplyStaTuning();
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
      esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
#endif

      bool ok = wifiStoreTrySoftReconnect(prefs);
#if WIFI_RECOVERY_ALLOW_SCAN
      if (!ok) ok = wifiStoreTryConnect(prefs);
#endif
#if WIFI_RECOVERY_NONBLOCK
      if (!ok) {
        wifiKickAt = millis();
        Serial.println("WiFi soft kick issued (non-blocking)");
      }
#endif

      if (ok || WiFi.status() == WL_CONNECTED) {
        wifiFailStreak = 0;
        wifiKickAt = 0;
        blePausedForWifi = false;
        wifiDownSince = 0;
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
        // Stay WiFi-biased — BALANCE after reconnect was letting BLE starve STA.
        esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
#endif
        Serial.println("WiFi reconnected");
        startMdns();
        mqttConnect();
      } else {
#if !WIFI_RECOVERY_NONBLOCK
        wifiFailStreak++;
        Serial.printf("WiFi reconnect failed (%u/%u)\n",
                      wifiFailStreak, WIFI_RECONNECT_FAIL_RESTART);
        if (wifiFailStreak >= WIFI_RECONNECT_FAIL_RESTART) {
          Serial.println("WiFi reconnect exhausted, restarting...");
          delay(500);
          ESP.restart();
        }
#endif
      }
      }
    }
  } else {
    // Connected: keep STA alive, but don't starve Basen BLE with permanent WIFI bias.
    if (wifiDownSince || blePausedForWifi) {
      wifiDownSince = 0;
      blePausedForWifi = false;
    }
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
    static unsigned long lastCoexWifi = 0;
    if (millis() - lastCoexWifi > 5000) {
      lastCoexWifi = millis();
      // Classic single radio: while Basen is up, prefer BT briefly then balance.
      // Permanent WIFI preference starves GATT notifies within seconds.
      if (bmsMgr.connected) {
        if (gBlePreferBtUntil && (int32_t)(millis() - gBlePreferBtUntil) < 0)
          esp_coex_preference_set(ESP_COEX_PREFER_BT);
        else
          esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);
      } else {
        esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
      }
      WiFi.setSleep(false);
    }
#endif
#if WIFI_ROAM_CHECK_MS > 0
    // Never roam/scan while BMS is linked — scan drops Classic BLE.
    static unsigned long lastRoamCheck = 0;
    const bool due = gForceWifiRoam || (millis() - lastRoamCheck > WIFI_ROAM_CHECK_MS);
    if (due && !bmsMgr.connected) {
      gForceWifiRoam = false;
      lastRoamCheck = millis();
      int curRssi = WiFi.RSSI();
      if (curRssi < WIFI_ROAM_MIN_RSSI) {
        Serial.printf("WiFi weak (%d dBm) — picking strongest saved AP\n", curRssi);
        String before = WiFi.SSID();
        if (wifiStoreTryBestSaved(prefs, WIFI_ROAM_IMPROVE_DB) && WiFi.SSID() != before) {
          Serial.printf("WiFi roamed %s (%d) -> %s (%d)\n",
                        before.c_str(), curRssi, WiFi.SSID().c_str(), WiFi.RSSI());
          startMdns();
          mqttConnect();
          publishStatus();
        }
      }
    } else if (due) {
      gForceWifiRoam = false;
      lastRoamCheck = millis();
    }
#endif
  }

  // Classic: track continuous WiFi-up time before allowing BLE.
  static unsigned long wifiStableSince = 0;
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiStableSince) wifiStableSince = millis();
  } else {
    wifiStableSince = 0;
  }
#if defined(ESP32_SLIM_BUILD)
  const bool bleRadioOk = !blePausedForWifi
      && wifiStableSince
      && (millis() - wifiStableSince >= BLE_WIFI_STABLE_MS);
#else
  const bool bleRadioOk = !blePausedForWifi;
#endif

  if (!mqtt.connected()) {
    static unsigned long lastTry = 0;
    if (WiFi.status() == WL_CONNECTED && millis() - lastTry > 10000) {
      lastTry = millis();
      mqttConnect();
    }
  }
  // Always build/publish status on interval — local Mosquitto AND HiveMQ Pages mirror.
  if (WiFi.status() == WL_CONNECTED && millis() - lastMqttPublish > MQTT_PUBLISH_INTERVAL_MS) {
    lastMqttPublish = millis();
    publishStatus();
  }

  // Cloud live first (cached), then one Solarman step, then cloud again —
  // so 100ms publishes are not stuck behind a TCP register read.
  pvxCloud.loop(deyeMgr, bmsMgr, deviceId, FIRMWARE_VERSION);
  deyeMgr.loop();
  pvxCloud.loop(deyeMgr, bmsMgr, deviceId, FIRMWARE_VERSION);



#ifndef ESP32_SLIM_BUILD
  if (gModbusScanPending && !otaInProgress()) {
    gModbusScanPending = false;
    Serial.println("Running Modbus scan...");
    bool found = genMgr.scanBus(1, 8);
    if (found) {
      genMgr.enabled = true;
      genMgr.save(prefs);
      genMgr.pollOnce();
    }
    Serial.println(genMgr.data.lastScan);
    publishGensetMqtt();
    publishStatus();
  }

  if (gModbusLoopbackPending && !otaInProgress()) {
    gModbusLoopbackPending = false;
    genMgr.data.lastScan = genMgr.loopbackTest();
    Serial.println(genMgr.data.lastScan);
    genMgr.applyBaud();
    publishGensetMqtt();
    publishStatus();
  }
#endif

  if (gWifiScanPending && !otaInProgress()) {
    gWifiScanPending = false;
    Serial.println("Running WiFi scan...");
    publishWifiScan();
  }

  if (gWifiConnectPending && !otaInProgress()) {
    gWifiConnectPending = false;
    runWifiConnectJob();
  }

  if (gBleScanPending && !otaInProgress()) {
#if defined(ESP32_SLIM_BUILD)
    if (!bleRadioOk) {
      // Defer scan until WiFi has been stable — sync BLE scan drops STA.
    } else
#endif
    {
      gBleScanPending = false;
      Serial.println("Running BLE scan...");
      publishBleScan();
    }
  }

  if (gBleConnectPending && !otaInProgress()) {
#if defined(ESP32_SLIM_BUILD)
    if (!bleRadioOk) {
      // Keep pending until WiFi has been stable — BLE connect blocks Classic loop.
    } else
#endif
    {
      gBleConnectPending = false;
      runBleConnectJob();
    }
  }

  // Auto-reconnect / duty-cycle BMS after WiFi has been stable (bleRadioOk).
#if BLE_DUTY_CYCLE
  // Classic: connect briefly, publish snapshot, disconnect — continuous link cannot survive WiFi coex.
  {
    static unsigned long dutyIdleUntil = 0;
    static unsigned long dutyHoldUntil = 0;
    static bool dutyActive = false;
    static bool dutyGotSnapshot = false;

    if (!bleRadioOk || bmsMgr.mac.length() == 0 || WiFi.RSSI() < BLE_RECONNECT_MIN_RSSI) {
      if (dutyActive && bmsMgr.connected) {
        if (bmsMgr.bms.valid) publishBmsMqtt();
        bmsMgr.dropLink(true, bmsMgr.bms.valid);
      }
      dutyActive = false;
      dutyGotSnapshot = false;
    } else if (!dutyActive && millis() >= dutyIdleUntil) {
      Serial.printf("BLE duty connect: %s (wifi rssi=%d)\n", bmsMgr.mac.c_str(), WiFi.RSSI());
      pumpNetwork();
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
      gBlePreferBtUntil = millis() + BLE_DUTY_HOLD_MS + 3000;
      esp_coex_preference_set(ESP_COEX_PREFER_BT);
#endif
      bool ok = bmsMgr.connect(bmsMgr.type, bmsMgr.name, bmsMgr.mac, prefs);
      pumpNetwork();
      if (ok) {
        dutyActive = true;
        dutyGotSnapshot = false;
        dutyHoldUntil = millis() + BLE_DUTY_HOLD_MS;
      } else {
        dutyIdleUntil = millis() + BLE_DUTY_IDLE_MS;
      }
    } else if (dutyActive) {
      if (bmsMgr.connected) {
        bmsMgr.poll();
        pumpNetwork();
        if (bmsMgr.bms.valid) dutyGotSnapshot = true;
      }
      const bool timedOut = millis() >= dutyHoldUntil;
      const bool linkDown = !bmsMgr.connected;
      // Keep the BLE session for nearly the full hold window so MQTT can publish often.
      // Only end early in the last 300ms once we already have a valid snapshot.
      if (timedOut || linkDown || (dutyGotSnapshot && (int32_t)(dutyHoldUntil - millis()) < 300)) {
        if (bmsMgr.bms.valid) publishBmsMqtt();
        bool keep = bmsMgr.bms.valid || dutyGotSnapshot;
        if (bmsMgr.connected) bmsMgr.dropLink(true, keep);
        else if (keep) bmsMgr.bms.valid = true;
        dutyActive = false;
        dutyGotSnapshot = false;
        dutyIdleUntil = millis() + BLE_DUTY_IDLE_MS;
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
        esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
#endif
        Serial.println("BLE duty session end");
      }
    }
  }
#elif BLE_AUTO_RECONNECT
  if (bleRadioOk && bmsMgr.mac.length() > 0 && !bmsMgr.connected
      && millis() - lastBleReconnect > BLE_RECONNECT_MS) {
    // Weak WiFi + blocking BLE connect kills the LAN web server.
    if (WiFi.RSSI() < BLE_RECONNECT_MIN_RSSI) {
      lastBleReconnect = millis();
      Serial.printf("BLE reconnect deferred (wifi rssi=%d) — roam first\n", WiFi.RSSI());
      gForceWifiRoam = true;
    } else {
      lastBleReconnect = millis();
      Serial.printf("BLE auto-reconnect: %s (wifi rssi=%d)\n", bmsMgr.mac.c_str(), WiFi.RSSI());
      pumpNetwork();
      bool ok = bmsMgr.connect(bmsMgr.type, bmsMgr.name, bmsMgr.mac, prefs);
      pumpNetwork();
      if (ok) {
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
        gBlePreferBtUntil = millis() + 20000;
        esp_coex_preference_set(ESP_COEX_PREFER_BT);
#endif
      } else if (BLE_RECONNECT_FAIL_BACKOFF_MS > BLE_RECONNECT_MS) {
        // Push next attempt further out after a failed connect.
        lastBleReconnect = millis() + (BLE_RECONNECT_FAIL_BACKOFF_MS - BLE_RECONNECT_MS);
      }
    }
  }
#endif

  static unsigned long lastBmsMqtt = 0;

#if BLE_DUTY_CYCLE
  // Publish while linked, and keep publishing last-good data briefly across reconnect gaps
  // so Home Assistant does not freeze for many seconds when BLE drops.
  if (bmsMgr.bms.valid && millis() - lastBmsMqtt > MQTT_BMS_PUBLISH_INTERVAL_MS) {
    const bool freshLink = bmsMgr.connected;
    const bool grace = bmsMgr.lastCellMs && (millis() - bmsMgr.lastCellMs) < 12000;
    if (freshLink || grace) {
      lastBmsMqtt = millis();
      publishBmsMqtt();
    }
  }
#else
  if (bmsMgr.bms.valid && millis() - lastBmsMqtt > MQTT_BMS_PUBLISH_INTERVAL_MS) {
    const bool freshLink = bmsMgr.connected;
    const bool grace = bmsMgr.lastCellMs && (millis() - bmsMgr.lastCellMs) < 12000;
    if (freshLink || grace) {
      lastBmsMqtt = millis();
      publishBmsMqtt();
    }
  }

  if (bleRadioOk) {
    bmsMgr.maintain();
  }
#endif

#ifndef ESP32_SLIM_BUILD
  genMgr.poll();
  genMgr.loopTickDelays();

  genSched.tick(genMgr, prefs);

  if (meterMgr.enabled && genMgr.profile == MODBUS_PROFILE_PS0600)
    meterMgr.poll();

  static unsigned long lastGenMqtt = 0;

  if (genMgr.enabled && (genMgr.takePublishPending()
      || genMgr.delayCountdownActive()
      || (genMgr.data.pollComplete && millis() - lastGenMqtt > MODBUS_POLL_INTERVAL_MS))) {
    lastGenMqtt = millis();
    publishGensetMqtt();
  }

  static unsigned long lastMeterMqtt = 0;
  if (meterMgr.enabled && genMgr.profile == MODBUS_PROFILE_PS0600
      && (meterMgr.takePublishPending()
          || (meterMgr.data.pollComplete && millis() - lastMeterMqtt > 5000))) {
    lastMeterMqtt = millis();
    publishMeterMqtt();
  }
#endif

}


