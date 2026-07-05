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
  unsigned long lastNotifyMs = 0;
  unsigned long lastCellMs = 0;
  unsigned long lastKickMs = 0;
  unsigned long connectedSinceMs = 0;
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
    lastCellMs = 0;
    connectedSinceMs = 0;
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

  void registerNotify() {
    if (chrNotify && chrNotify->canNotify()) chrNotify->registerForNotify(notifyThunk);
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
    registerNotify();
    return true;
  }

  bool writeBytes(const uint8_t* data, size_t len) {
    if (!chrWrite || !client || !client->isConnected()) return false;
    return chrWrite->writeValue((uint8_t*)data, len, false);
  }

  void onNotify(uint8_t* data, size_t len) {
    lastNotifyMs = millis();
    bool gotCell = jkFeed(proto, data, len, bms);
    if (gotCell) lastCellMs = millis();
    if (gotCell || bms.valid) {
      type = BmsType::Jk;
      bms.type = type;
      bms.connected = true;
      lastDisplay = bmsToDisplay(bms);
    } else if (!bms.valid && len > 0) {
      lastDisplay = "rx " + String((int)len) + "B";
    }
  }

  void sendPollCmd(uint8_t cmd) {
    uint8_t buf[32];
    size_t n = jkBuildCmd(cmd, buf);
    if (!writeBytes(buf, n)) {
      Serial.println("BMS write failed");
      dropLink();
    }
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
    registerNotify();
    sendPollCmd(0x96);
  }

  void poll() {
    if (!connected || type != BmsType::Jk) return;
    if (millis() - lastPoll < pollIntervalMs) return;
    lastPoll = millis();
    sendPollCmd(0x96);
  }

  void maintain() {
    if (!connected || type != BmsType::Jk) return;

    if (!client || !client->isConnected()) {
      Serial.println("BMS BLE link down");
      dropLink();
      return;
    }

    unsigned long now = millis();

    if (connectedSinceMs && now - connectedSinceMs > BLE_SESSION_REFRESH_MS) {
      Serial.println("BMS proactive session refresh");
      dropLink();
      return;
    }

    poll();

    if (lastCellMs && now - lastCellMs > BLE_CELL_STALE_MS) {
      Serial.printf("BMS no cell frame %lus\n", (now - lastCellMs) / 1000);
      dropLink();
      return;
    }

    unsigned long notifyAge = lastNotifyMs ? now - lastNotifyMs : 0xFFFFFFFF;
    if (notifyAge > BLE_NOTIFY_RESET_MS) {
      Serial.printf("BMS no notify %lus, reconnect\n", notifyAge / 1000);
      dropLink();
      return;
    }
    if (notifyAge > BLE_NOTIFY_KICK_MS && now - lastKickMs > 3000) {
      lastKickMs = now;
      Serial.println("BMS kick");
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
    connectedSinceMs = millis();
    bms.connected = true;
    bleRssi = client->getRssi();
    lastNotifyMs = millis();
    prefs.putString("ble_mac", mac);
    prefs.putString("ble_name", name);
    prefs.putString("bms_type", bmsTypeId(type));

    delay(200);
    sendInitialPolls();
    Serial.printf("BMS connected [%s] rssi=%d\n", mac.c_str(), bleRssi);
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
