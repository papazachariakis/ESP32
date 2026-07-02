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

#include <ESPmDNS.h>
#include <esp_mac.h>
#include <Preferences.h>

#include <PubSubClient.h>

#include <WebServer.h>

#include <BLEDevice.h>

#include <BLEScan.h>

#include <BLEClient.h>

#include <ArduinoJson.h>

#include "config.h"

#include "webui.h"
#include "remote_ui.h"
#include "bms_manager.h"
#include "wifi_store.h"
#include "ota.h"



WebServer server(80);

WiFiClient wifiClient;

PubSubClient mqtt(wifiClient);

Preferences prefs;

BmsManager bmsMgr;



String deviceId;

String mqttBroker = MQTT_DEFAULT_BROKER;

uint16_t mqttPort = MQTT_DEFAULT_PORT;

String topicStatus, topicCmd, topicBms;

String bleScanJson = "[]";



bool relayState[RELAY_COUNT] = { false };

unsigned long lastMqttPublish = 0;

unsigned long lastBleReconnect = 0;



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
  WiFi.macAddress(mac);
  if (mac[0] == 0 && mac[1] == 0 && mac[2] == 0 && mac[3] == 0 && mac[4] == 0 && mac[5] == 0) {
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
  }
  char buf[13];
  snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
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

  doc["ip"] = WiFi.localIP().toString();

  doc["wifi_ssid"] = WiFi.SSID();

  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;

  doc["rssi"] = WiFi.RSSI();

  JsonArray wifiSaved = doc.createNestedArray("wifi_saved");
  doc["wifi_saved_count"] = wifiStoreAddToJson(prefs, wifiSaved);



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

  ble["data"] = bmsMgr.lastDisplay;

  ble["bms_type"] = bmsTypeId(bmsMgr.type);

  ble["bms_label"] = bmsTypeLabel(bmsMgr.type);

  ble["basen"] = bmsMgr.type == BmsType::Tianpower;



  JsonObject bmsObj = doc.createNestedObject("bms");

  bmsFillJson(bmsObj, bmsMgr.bms);



  JsonObject mq = doc.createNestedObject("mqtt");

  mq["broker"] = mqttBroker;

  mq["port"] = mqttPort;

  mq["connected"] = mqtt.connected();

  mq["topic_status"] = topicStatus;

  mq["topic_cmd"] = topicCmd;

  mq["topic_bms"] = topicBms;



  String out;

  serializeJson(doc, out);

  return out;

}



void publishStatus() {

  if (!mqtt.connected()) return;

  mqtt.publish(topicStatus.c_str(), buildStatusJson().c_str());

}



void mqttCallback(char* topic, byte* payload, unsigned int length) {

  StaticJsonDocument<256> doc;

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

}



bool mqttConnect() {

  if (mqttBroker.isEmpty()) return false;

  mqtt.setServer(mqttBroker.c_str(), mqttPort);

  mqtt.setCallback(mqttCallback);

  mqtt.setBufferSize(4096);



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

  for (int i = 0; i < RELAY_COUNT; i++) {

    relayState[i] = prefs.getBool(("r" + String(i)).c_str(), false);

    digitalWrite(RELAY_PINS[i], relayState[i] ? HIGH : LOW);

  }

}



void setupWiFi() {

  WiFi.mode(WIFI_STA);

  bool forcePortal = prefs.getBool(WIFI_FORCE_PORTAL_KEY, false);
  if (forcePortal) prefs.putBool(WIFI_FORCE_PORTAL_KEY, false);

  if (!forcePortal && wifiStoreTryConnect(prefs)) {
    Serial.print("WiFi OK (saved list): ");
    Serial.println(WiFi.localIP());
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

    o["basen"] = bt == BmsType::Tianpower;

  }

  bleScanJson = "";

  serializeJson(arr, bleScanJson);

  scan->clearResults();

  Serial.println("BLE scan done");

}



void handleRoot() {
  server.send_P(200, "text/html", WEB_UI);
}

void handleRemoteApp() {
  server.send_P(200, "text/html", REMOTE_UI);
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
  if (bt == BmsType::None && name.startsWith("TP_")) bt = BmsType::Tianpower;
  if (bt == BmsType::None && bmsMgr.type != BmsType::None) bt = bmsMgr.type;

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
  server.on("/remote", HTTP_GET, handleRemoteApp);
  server.on("/app", HTTP_GET, handleRemoteApp);

  server.on("/api/status", HTTP_GET, handleStatus);

  server.on("/api/relay", HTTP_POST, handleRelay);

  server.on("/api/relay/alloff", HTTP_POST, handleAllOff);

  server.on("/api/ble/scan", HTTP_GET, handleBleScan);

  server.on("/api/ble/connect", HTTP_POST, handleBleConnect);

  server.on("/api/ble/disconnect", HTTP_POST, handleBleDisconnect);

  server.on("/api/wifi/reset", HTTP_POST, handleWifiReset);

  server.on("/api/mqtt", HTTP_POST, handleMqttSave);
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



  BLEDevice::init("ESP32-Control");



  setupWiFi();



  if (MDNS.begin("esp32")) {

    Serial.println("mDNS: http://esp32.local");

  }



  setupRoutes();
  setupArduinoOta();
  mqttConnect();



  if (bmsMgr.mac.length() > 0) {

    Serial.println("Reconnecting BMS BLE: " + bmsMgr.mac);

    bmsMgr.connect(bmsMgr.type, bmsMgr.name, bmsMgr.mac, prefs);

  }



  Serial.println("ESP32 Control Hub ready");

  Serial.println("Open http://esp32.local or " + WiFi.localIP().toString());

}



void loop() {
  server.handleClient();
  ArduinoOTA.handle();

  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastWifiTry = 0;
    if (millis() - lastWifiTry > 45000) {
      lastWifiTry = millis();
      if (wifiStoreTryConnect(prefs)) {
        Serial.println("WiFi reconnected via saved list");
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
    mqtt.loop();
    if (millis() - lastMqttPublish > MQTT_PUBLISH_INTERVAL_MS) {
      lastMqttPublish = millis();
      publishStatus();
    }
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



  bmsMgr.poll();

}


