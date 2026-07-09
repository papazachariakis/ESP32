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
#include <ESPmDNS.h>
#include <Preferences.h>

#include <PubSubClient.h>

#include <WebServer.h>

#include <BLEDevice.h>

#include <BLEScan.h>

#include <BLEClient.h>

#include <ArduinoJson.h>

#include "config.h"

#include "webui.h"
#include "bms_manager.h"
#include "wifi_store.h"
#include "hub_reset.h"
#include "ota.h"
#include "cummins_gen.h"



WebServer server(80);

WiFiClient wifiClient;

PubSubClient mqtt(wifiClient);

Preferences prefs;

BmsManager bmsMgr;
GenManager genMgr;



String deviceId;

String mqttBroker = MQTT_DEFAULT_BROKER;

uint16_t mqttPort = MQTT_DEFAULT_PORT;

String topicStatus, topicCmd, topicBms, topicGenset, topicWifi;

String bleScanJson = "[]";



bool relayState[RELAY_COUNT] = { false };

unsigned long lastMqttPublish = 0;

unsigned long lastBleReconnect = 0;

volatile bool gModbusScanPending = false;

volatile bool gModbusLoopbackPending = false;

volatile bool gWifiScanPending = false;

volatile bool gWifiConnectPending = false;

String gWifiConnectSsid;

String gWifiConnectPass;

void pumpNetwork() {
  server.handleClient();
  // Web OTA upload handled in server routes
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
  if (!mqtt.connected() || !genMgr.enabled) return;
  StaticJsonDocument<2560> doc;
  JsonObject root = doc.to<JsonObject>();
  genFillJson(root, genMgr.data, genMgr.profile);
  root["enabled"] = genMgr.enabled;
  char payload[2560];
  serializeJson(doc, payload);
  mqtt.publish(topicGenset.c_str(), payload);
}

void publishBmsMqtt() {

  if (!mqtt.connected() || !bmsMgr.bms.valid) return;

  StaticJsonDocument<2048> doc;

  JsonObject root = doc.to<JsonObject>();

  bmsFillJson(root, bmsMgr.bms);

  char payload[2048];

  serializeJson(doc, payload);

  mqtt.publish(topicBms.c_str(), payload);

}



String buildStatusJson() {

  StaticJsonDocument<6144> doc;

  doc["device_id"] = deviceId;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["board"] = BOARD_ID;
  doc["board_label"] = BOARD_LABEL;

  doc["ip"] = WiFi.localIP().toString();

  doc["wifi_ssid"] = WiFi.SSID();

  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;

  doc["rssi"] = WiFi.RSSI();

  JsonArray savedWifi = doc.createNestedArray("wifi_saved");
  wifiStoreAddToJson(prefs, savedWifi);

  if (otaStatusField()) doc["ota_phase"] = otaStatusField();
  if (lastOtaError().length()) doc["ota_error"] = lastOtaError();
  if (mqttOtaActive()) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%d/%d", mqttOtaReceived(), mqttOtaExpected());
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
  ble["data_age_ms"] = (bmsMgr.connected && bmsMgr.bms.valid)
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

  mq["connected"] = mqtt.connected();

  mq["topic_status"] = topicStatus;

  mq["topic_cmd"] = topicCmd;

  mq["topic_bms"] = topicBms;

  mq["topic_genset"] = topicGenset;

  JsonObject genset = doc.createNestedObject("genset");
  genFillJson(genset, genMgr.data, genMgr.profile);
  genset["enabled"] = genMgr.enabled;
  genset["slave_id"] = genMgr.slaveId;
  genset["baud"] = genMgr.baud;
  genset["probe_reg"] = genMgr.probeReg;
  genset["modbus_rx"] = MODBUS_RX_PIN;
  genset["modbus_tx"] = MODBUS_TX_PIN;
  genset["modbus_de"] = MODBUS_DE_PIN;



  String out;

  serializeJson(doc, out);

  return out;

}



void publishStatus() {

  if (!mqtt.connected()) return;

  mqtt.publish(topicStatus.c_str(), buildStatusJson().c_str());

}

void publishWifiScan() {
  if (!mqtt.connected()) return;

  int found = WiFi.scanNetworks(false, true);
  StaticJsonDocument<2048> doc;
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
  doc["current"] = WiFi.SSID();
  doc["ip"] = WiFi.localIP().toString();
  String out;
  serializeJson(doc, out);
  mqtt.publish(topicWifi.c_str(), out.c_str(), false);
  WiFi.scanDelete();
}

void publishWifiResult(bool ok, const char* errorMsg = nullptr) {
  if (!mqtt.connected()) return;
  StaticJsonDocument<256> doc;
  doc["ok"] = ok;
  if (ok) {
    doc["current"] = WiFi.SSID();
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

  StaticJsonDocument<512> doc;

  if (deserializeJson(doc, payload, length)) return;



  if (doc.containsKey("relay")) {

    if (doc["relay"] == "all") {

      allRelaysOff();

    } else {

      int idx = doc["relay"].as<int>();

      bool on = doc["on"] | false;

      setRelay(idx, on);

    }

    publishStatus();

  }

  if (doc.containsKey("genset")) {
    const char* action = doc["genset"];
    if (action && genMgr.runGensetCmd(action)) {
      genMgr.pollOnce();
      publishGensetMqtt();
      publishStatus();
    }
  }

  if (doc.containsKey("modbus_cfg")) {
    JsonObject cfg = doc["modbus_cfg"];
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
    Serial.printf("MQTT: modbus cfg baud=%u id=%u\n", genMgr.baud, genMgr.slaveId);
    if (genMgr.enabled) genMgr.pollOnce();
    publishGensetMqtt();
    publishStatus();
  }

  if (doc.containsKey("modbus_scan")) {
    const char* pw = doc["modbus_scan"];
    if (!pw || remoteOtaPasswordOk(pw) || doc["modbus_scan"].is<bool>()) {
      Serial.println("MQTT: modbus scan requested");
      gModbusScanPending = true;
    }
  }

  if (doc.containsKey("modbus_loopback")) {
    Serial.println("MQTT: modbus loopback requested");
    gModbusLoopbackPending = true;
  }

  if (doc.containsKey("reboot")) {
    const char* pw = doc["reboot"];
    if (remoteOtaPasswordOk(pw)) {
      Serial.println("MQTT: reboot requested");
      publishStatus();
      delay(300);
      ESP.restart();
    }
  }

  if (doc.containsKey("ota")) {
    const char* pw = doc["ota"];
    if (remoteOtaPasswordOk(pw)) {
      Serial.println("MQTT: remote OTA queued");
      requestRemoteOta();
    }
  }

  if (doc.containsKey("ota_http") && doc.containsKey("url")) {
    const char* pw = doc["ota_http"];
    const char* url = doc["url"];
    if (remoteOtaPasswordOk(pw) && url) {
      Serial.println("MQTT: HTTP OTA queued");
      requestHttpOta(url);
    }
  }

  if (doc.containsKey("ota_mqtt")) {
    const char* pw = doc["ota_mqtt"];
    if (remoteOtaPasswordOk(pw)) {
      int size = doc["size"] | 0;
      Serial.printf("MQTT: chunk OTA begin %d\n", size);
      mqttOtaBegin(size);
    }
  }

  if (doc.containsKey("ota_end")) {
    const char* pw = doc["ota_end"];
    if (remoteOtaPasswordOk(pw)) {
      Serial.println("MQTT: chunk OTA end");
      mqttOtaFinish();
    }
  }

  if (doc.containsKey("wifi_scan")) {
    Serial.println("MQTT: wifi scan requested");
    gWifiScanPending = true;
  }

  if (doc.containsKey("wifi_connect")) {
    JsonObject w = doc["wifi_connect"];
    const char* ssid = w["ssid"];
    if (ssid && ssid[0]) {
      gWifiConnectSsid = ssid;
      gWifiConnectPass = w["pass"] | "";
      gWifiConnectPending = true;
      Serial.printf("MQTT: wifi connect %s\n", ssid);
    }
  }

  if (doc.containsKey("factory_reset")) {
    const char* pw = doc["factory_reset"];
    if (remoteOtaPasswordOk(pw)) {
      Serial.println("MQTT: factory reset requested");
      publishStatus();
      delay(300);
      performFactoryReset(prefs);
    }
  }

}



bool mqttConnect() {

  if (mqttBroker.isEmpty()) return false;

  mqtt.setServer(mqttBroker.c_str(), mqttPort);

  mqtt.setCallback(mqttCallback);

  mqtt.setBufferSize(3072);



  String clientId = "esp32-" + deviceId;

  if (mqtt.connect(clientId.c_str())) {

    mqtt.subscribe(topicCmd.c_str());

    publishStatus();

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

  bmsMgr.mac = prefs.getString("ble_mac", "");

  bmsMgr.name = prefs.getString("ble_name", "");

  bmsMgr.type = bmsTypeFromString(prefs.getString("bms_type", ""));
  if (bmsMgr.type == BmsType::None && bmsMgr.name.length())
    bmsMgr.type = bmsDetectFromName(bmsMgr.name);
  if (bmsMgr.type == BmsType::None && bmsMgr.mac.length()) bmsMgr.type = BmsType::Jk;

  genMgr.load(prefs);
  genMgr.pollIntervalMs = MODBUS_POLL_INTERVAL_MS;

  for (int i = 0; i < RELAY_COUNT; i++) {

    relayState[i] = prefs.getBool(("r" + String(i)).c_str(), false);

    digitalWrite(RELAY_PINS[i], relayState[i] ? HIGH : LOW);

  }

}



void setupWiFi() {

  WiFi.mode(WIFI_STA);

  wifiStoreSeedDefaults(prefs);

  bool forcePortal = prefs.getBool(WIFI_FORCE_PORTAL_KEY, false);
  if (forcePortal) prefs.putBool(WIFI_FORCE_PORTAL_KEY, false);

  if (!forcePortal && wifiStoreTryConnect(prefs)) {
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

  wifiStoreUpsert(prefs, WiFi.SSID(), wm.getWiFiPass(false));

  Serial.print("WiFi OK: ");

  Serial.println(WiFi.localIP());
  startMdns();

}



void startBleScan() {

  BLEScan* scan = BLEDevice::getScan();

  scan->setActiveScan(true);

  scan->clearResults();

  BLEScanResults* results = scan->start(BLE_SCAN_SECONDS, false);



  StaticJsonDocument<6144> doc;

  JsonArray arr = doc.to<JsonArray>();

  int count = results ? results->getCount() : 0;

  for (int i = 0; i < count; i++) {

    BLEAdvertisedDevice d = results->getDevice(i);

    String name = d.getName().c_str();

    BmsType bt = bmsDetectType(name, d);

    if (bt == BmsType::None) continue;

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

  Serial.println("BLE scan done");

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

  setRelay(doc["index"] | 0, doc["on"] | false);

  publishStatus();

  server.send(200, "application/json", "{\"ok\":true}");

}



void handleAllOff() {

  allRelaysOff();

  publishStatus();

  server.send(200, "application/json", "{\"ok\":true}");

}



void handleBleScan() {

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

  String mac = doc["mac"] | "";

  String name = doc["name"] | "";

  String typeStr = doc["type"] | "auto";

  if (mac.length() < 11) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid mac\"}");
    return;
  }

  BmsType bt = BmsType::None;
  if (typeStr != "auto" && typeStr.length()) bt = bmsTypeFromString(typeStr);
  if (bt == BmsType::None) bt = bmsDetectFromName(name);
  if (bt == BmsType::None && typeStr == "basen") bt = BmsType::Basen;
  if (bt == BmsType::None && typeStr == "jk") bt = BmsType::Jk;
  if (bt == BmsType::None) bt = BmsType::Jk;

  if (bt == BmsType::None) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"unknown_bms_type\"}");
    return;
  }

  Serial.printf("BLE connect request: %s [%s] %s\n", name.c_str(), bmsTypeId(bt), mac.c_str());

  bool ok = bmsMgr.connect(bt, name, mac, prefs);

  if (ok) {
    publishBmsMqtt();
    publishStatus();
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }

  server.send(500, "application/json", "{\"ok\":false,\"error\":\"connect_failed\"}");

}



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

  const char* action = doc["action"];

  if (!action || !genMgr.runGensetCmd(action)) {

    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_action\"}");

    return;

  }

  genMgr.pollOnce();

  publishGensetMqtt();

  publishStatus();

  server.send(200, "application/json", "{\"ok\":true}");

}



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

  genMgr.enabled = doc["enabled"] | true;

  genMgr.slaveId = (uint8_t)(doc["slave_id"] | 1);

  if (doc.containsKey("probe_reg")) genMgr.probeReg = (uint16_t)(doc["probe_reg"].as<int>());

  genMgr.baud = doc["baud"] | 9600;

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

  if (genMgr.enabled) genMgr.pollOnce();

  server.send(200, "application/json", "{\"ok\":true}");

}



void handleModbusScan() {
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



void handleModbusLoopback() {
  String r = genMgr.loopbackTest();
  genMgr.applyBaud();
  StaticJsonDocument<160> doc;
  doc["ok"] = r.startsWith("LOOPBACK OK");
  doc["result"] = r;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}



void handleBleDisconnect() {

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

  StaticJsonDocument<256> doc;

  deserializeJson(doc, server.arg("plain"));

  mqttBroker = doc["broker"] | MQTT_DEFAULT_BROKER;

  mqttPort = doc["port"] | MQTT_DEFAULT_PORT;

  prefs.putString("mqtt_broker", mqttBroker);

  prefs.putUInt("mqtt_port", mqttPort);

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

  server.on("/api/ble/scan", HTTP_GET, handleBleScan);

  server.on("/api/ble/connect", HTTP_POST, handleBleConnect);

  server.on("/api/ble/disconnect", HTTP_POST, handleBleDisconnect);

  server.on("/api/wifi/reset", HTTP_POST, handleWifiReset);
  server.on("/api/factory/reset", HTTP_POST, handleFactoryReset);

  server.on("/api/mqtt", HTTP_POST, handleMqttSave);
  server.on("/api/genset", HTTP_GET, handleGenset);
  server.on("/api/genset/cmd", HTTP_POST, handleGensetCmd);
  server.on("/api/modbus", HTTP_POST, handleModbusSave);
  server.on("/api/modbus/scan", HTTP_POST, handleModbusScan);
  server.on("/api/modbus/loopback", HTTP_POST, handleModbusLoopback);
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

  topicGenset = "home/" + deviceId + "/genset";

  topicWifi = "home/" + deviceId + "/wifi";



  BLEDevice::init("ESP32-Control");



  setupWiFi();

  genMgr.begin();



  setupRoutes();
  setupArduinoOta();
  otaPrepHook() = []() {
    if (bmsMgr.client && bmsMgr.client->isConnected()) {
      bmsMgr.client->disconnect();
      bmsMgr.connected = false;
    }
  };
  otaStatusHook() = []() { publishStatus(); };
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
  Serial.printf("Modbus RX=%d TX=%d DE=%d\n", MODBUS_RX_PIN, MODBUS_TX_PIN, MODBUS_DE_PIN);

  Serial.println("Open http://" + WiFi.localIP().toString());

}



void loop() {
  pumpNetwork();

  if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
    mqtt.loop();
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

  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastWifiTry = 0;
    if (millis() - lastWifiTry > 45000) {
      lastWifiTry = millis();
      if (wifiStoreTryConnect(prefs)) {
        Serial.println("WiFi reconnected via saved list");
        startMdns();
        mqttConnect();
      }
    }
  }

  if (!mqtt.connected()) {
    static unsigned long lastTry = 0;
    if (WiFi.status() == WL_CONNECTED && millis() - lastTry > 10000) {
      lastTry = millis();
      mqttConnect();
    }
  } else {
    if (millis() - lastMqttPublish > MQTT_PUBLISH_INTERVAL_MS) {
      lastMqttPublish = millis();
      publishStatus();
    }
  }



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

  if (gWifiScanPending && !otaInProgress()) {
    gWifiScanPending = false;
    Serial.println("Running WiFi scan...");
    publishWifiScan();
  }

  if (gWifiConnectPending && !otaInProgress()) {
    gWifiConnectPending = false;
    runWifiConnectJob();
  }

  if (bmsMgr.mac.length() > 0 && !bmsMgr.connected && millis() - lastBleReconnect > BLE_RECONNECT_MS) {
    lastBleReconnect = millis();
    bmsMgr.connect(bmsMgr.type, bmsMgr.name, bmsMgr.mac, prefs);
  }

  static unsigned long lastBmsMqtt = 0;

  if (bmsMgr.bms.valid && millis() - lastBmsMqtt > MQTT_BMS_PUBLISH_INTERVAL_MS) {
    lastBmsMqtt = millis();
    publishBmsMqtt();
  }

  bmsMgr.maintain();

  genMgr.poll();

  static unsigned long lastGenMqtt = 0;

  if (genMgr.enabled && millis() - lastGenMqtt > MODBUS_POLL_INTERVAL_MS) {

    lastGenMqtt = millis();

    publishGensetMqtt();

  }

}


