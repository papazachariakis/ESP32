#pragma once

#include "bms_common.h"
#include "bms_protocols.h"
#include "config.h"
#include "ota.h"
#include <BLEClient.h>
#include <Preferences.h>

class BmsBleCallbacks : public BLEClientCallbacks {
  void onDisconnect(BLEClient* pclient) override;
};

struct BmsManager {
  BmsData bms;
  BmsProtoState proto;
  BmsType type = BmsType::None;
  BLEClient* client = nullptr;
  BLERemoteCharacteristic* chrNotify = nullptr;
  BLERemoteCharacteristic* chrWrite = nullptr;
  String mac;
  String name;
  String lastDisplay;
  bool connected = false;
  int bleRssi = -999;
  unsigned long lastPoll = 0;
  unsigned long lastRssiRead = 0;
  unsigned long lastNotifyMs = 0;
  unsigned long lastKickMs = 0;
  uint16_t pollIntervalMs = BLE_POLL_MS;

  static void notifyThunk(BLERemoteCharacteristic* chr, uint8_t* data, size_t len, bool isNotify) {
    if (gInstance) gInstance->onNotify(data, len);
  }

  static BmsManager* gInstance;

  void dropLink() {
    if (gInstance == this) gInstance = nullptr;
    if (client) {
      if (client->isConnected()) client->disconnect();
      delete client;
    }
    client = nullptr;
    chrNotify = nullptr;
    chrWrite = nullptr;
    connected = false;
    bleRssi = -999;
    proto = BmsProtoState();
    bms = BmsData();
    lastDisplay = "";
    lastNotifyMs = 0;
  }

  void reset() { dropLink(); }

  void handleDisconnect() {
    Serial.println("BMS BLE disconnected (callback)");
    dropLink();
  }

  void forgetIdentity() {
    type = BmsType::None;
    mac = "";
    name = "";
  }

  bool getJkChars(BLEClient* c) {
    BLERemoteService* svc = nullptr;
    for (int i = 0; i < 20 && !svc; i++) {
      svc = c->getService(BLEUUID(JK_SERVICE_UUID));
      if (!svc) delay(200);
    }
    if (!svc) return false;
    chrNotify = svc->getCharacteristic(BLEUUID(JK_CHAR_UUID));
    if (!chrNotify) return false;
    chrWrite = chrNotify;
    if (chrNotify->canNotify()) chrNotify->registerForNotify(notifyThunk);
    return true;
  }

  bool writeBytes(const uint8_t* data, size_t len) {
    if (!chrWrite || !client || !client->isConnected()) return false;
    chrWrite->writeValue((uint8_t*)data, len, false);
    return true;
  }

  void onNotify(uint8_t* data, size_t len) {
    lastNotifyMs = millis();
    bool parsed = jkFeed(proto, data, len, bms);
    if (parsed || bms.valid) {
      type = BmsType::Jk;
      bms.type = type;
      bms.connected = true;
      lastDisplay = bmsToDisplay(bms);
    } else if (!bms.valid) {
      String hex;
      for (size_t i = 0; i < len && i < 48; i++) {
        char b[4];
        snprintf(b, sizeof(b), "%02X ", data[i]);
        hex += b;
      }
      hex.trim();
      lastDisplay = hex;
    }
  }

  void sendPollCmd(uint8_t cmd) {
    uint8_t buf[32];
    size_t n = jkBuildCmd(cmd, buf);
    writeBytes(buf, n);
  }

  void sendInitialPolls() {
    sendPollCmd(0x97);
    delay(400);
    sendPollCmd(0x96);
    delay(150);
    sendPollCmd(0x96);
    lastNotifyMs = millis();
  }

  void kickPolls() {
    sendPollCmd(0x97);
    sendPollCmd(0x96);
  }

  void poll() {
    if (!connected || type != BmsType::Jk) return;
    if (millis() - lastPoll < pollIntervalMs) return;
    lastPoll = millis();
    sendPollCmd(0x96);
  }

  // Call every loop iteration while BMS is configured.
  void maintain() {
    if (!connected || type != BmsType::Jk) return;

    if (!client || !client->isConnected()) {
      Serial.println("BMS BLE link down");
      dropLink();
      return;
    }

    unsigned long now = millis();
    if (now - lastRssiRead > 20000) {
      lastRssiRead = now;
      bleRssi = client->getRssi();
    }

    poll();

    unsigned long notifyAge = lastNotifyMs ? now - lastNotifyMs : 0xFFFFFFFF;
    if (notifyAge > BLE_NOTIFY_RESET_MS) {
      Serial.printf("BMS no BLE data %lus, reconnect\n", notifyAge / 1000);
      dropLink();
      return;
    }
    if (notifyAge > BLE_NOTIFY_KICK_MS && now - lastKickMs > 4000) {
      lastKickMs = now;
      Serial.println("BMS stream slow, kick polls");
      kickPolls();
    }
  }

  bool connect(BmsType t, const String& devName, const String& devMac, Preferences& prefs) {
    if (otaInProgress()) return false;
    String saveName = devName.length() ? devName : name;
    String saveMac = devMac.length() ? devMac : mac;
    dropLink();
    if (t == BmsType::None) t = bmsDetectFromName(saveName);
    if (t == BmsType::None) t = BmsType::Jk;

    type = t;
    name = saveName;
    mac = saveMac;
    bms.type = type;
    bms.name = name;
    bms.mac = mac;

    BLEAddress addr(mac.c_str());
    client = BLEDevice::createClient();
    static BmsBleCallbacks s_bleCb;
    client->setClientCallbacks(&s_bleCb);
    if (!client->connect(addr)) {
      Serial.println("BMS BLE connect failed");
      dropLink();
      return false;
    }
    delay(400);

    if (!getJkChars(client)) {
      Serial.println("BMS service/char not found");
      dropLink();
      return false;
    }

    gInstance = this;
    connected = true;
    bms.connected = true;
    bleRssi = client->getRssi();
    lastRssiRead = millis();
    lastNotifyMs = millis();
    prefs.putString("ble_mac", mac);
    prefs.putString("ble_name", name);
    prefs.putString("bms_type", bmsTypeId(type));

    delay(200);
    sendInitialPolls();
    Serial.printf("BMS connected [%s]: %s rssi=%d\n", bmsTypeId(type), name.c_str(), bleRssi);
    return true;
  }

  void disconnect(Preferences& prefs) {
    dropLink();
    forgetIdentity();
    prefs.remove("ble_mac");
    prefs.remove("ble_name");
    prefs.remove("bms_type");
  }
};

inline void BmsBleCallbacks::onDisconnect(BLEClient* pclient) {
  if (BmsManager::gInstance) BmsManager::gInstance->handleDisconnect();
}

BmsManager* BmsManager::gInstance = nullptr;
