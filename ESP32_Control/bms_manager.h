#pragma once

#include "bms_common.h"
#include "bms_protocols.h"
#include "ota.h"
#include <BLEClient.h>
#include <Preferences.h>

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
  unsigned long lastPoll = 0;
  uint16_t pollIntervalMs = 4000;

  static void notifyThunk(BLERemoteCharacteristic* chr, uint8_t* data, size_t len, bool isNotify) {
    if (gInstance) gInstance->onNotify(data, len);
  }

  static BmsManager* gInstance;

  void reset() {
    if (gInstance == this) gInstance = nullptr;
    if (client) {
      if (client->isConnected()) client->disconnect();
      delete client;
    }
    client = nullptr;
    chrNotify = nullptr;
    chrWrite = nullptr;
    connected = false;
    type = BmsType::None;
    proto = BmsProtoState();
    bms = BmsData();
    mac = "";
    name = "";
    lastDisplay = "";
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

  void sendInitialPolls() {
    uint8_t buf[32];
    size_t n = jkBuildCmd(0x97, buf);
    writeBytes(buf, n);
    delay(600);
    n = jkBuildCmd(0x96, buf);
    writeBytes(buf, n);
    delay(200);
    n = jkBuildCmd(0x96, buf);
    writeBytes(buf, n);
  }

  void poll() {
    if (!connected || type != BmsType::Jk) return;
    if (millis() - lastPoll < pollIntervalMs) return;
    lastPoll = millis();

    uint8_t buf[32];
    size_t n = jkBuildCmd(0x96, buf);
    writeBytes(buf, n);
  }

  bool connect(BmsType t, const String& devName, const String& devMac, Preferences& prefs) {
    if (otaInProgress()) return false;
    String saveName = devName.length() ? devName : name;
    String saveMac = devMac.length() ? devMac : mac;
    reset();
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
    if (!client->connect(addr)) {
      Serial.println("BMS BLE connect failed");
      reset();
      return false;
    }
    delay(500);

    if (!getJkChars(client)) {
      Serial.println("BMS service/char not found");
      reset();
      return false;
    }

    gInstance = this;
    connected = true;
    bms.connected = true;
    prefs.putString("ble_mac", mac);
    prefs.putString("ble_name", name);
    prefs.putString("bms_type", bmsTypeId(type));

    delay(300);
    sendInitialPolls();
    Serial.printf("BMS connected [%s]: %s\n", bmsTypeId(type), name.c_str());
    return true;
  }

  void disconnect(Preferences& prefs) {
    reset();
    prefs.remove("ble_mac");
    prefs.remove("ble_name");
    prefs.remove("bms_type");
  }
};

BmsManager* BmsManager::gInstance = nullptr;
