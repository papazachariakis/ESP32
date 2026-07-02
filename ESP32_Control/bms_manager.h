#pragma once

#include "bms_common.h"
#include "tianpower_bms.h"
#include "bms_protocols.h"
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
  uint8_t pollIndex = 0;
  unsigned long lastPoll = 0;
  uint16_t pollIntervalMs = 1500;

  static void notifyThunk(BLERemoteCharacteristic* chr, uint8_t* data, size_t len, bool isNotify) {
    if (gInstance) gInstance->onNotify(data, len);
  }

  static BmsManager* gInstance;

  void reset() {
    if (client) {
      if (client->isConnected()) client->disconnect();
      delete client;
    }
    client = nullptr;
    chrNotify = nullptr;
    chrWrite = nullptr;
    connected = false;
    type = BmsType::None;
    pollIndex = 0;
    proto = BmsProtoState();
    bms = BmsData();
    mac = "";
    name = "";
    lastDisplay = "";
  }

  bool getChars(BLEClient* c, const char* svcUuid, const char* rxUuid, const char* txUuid) {
    BLERemoteService* svc = nullptr;
    for (int i = 0; i < 20 && !svc; i++) {
      svc = c->getService(BLEUUID(svcUuid));
      if (!svc) delay(200);
    }
    if (!svc) return false;
    chrNotify = svc->getCharacteristic(BLEUUID(rxUuid));
    chrWrite = svc->getCharacteristic(BLEUUID(txUuid));
    if (!chrNotify || !chrWrite) {
      if (chrNotify && chrNotify == chrWrite) {
        chrWrite = chrNotify;
      } else if (chrNotify && !chrWrite) {
        chrWrite = chrNotify;
      } else {
        return false;
      }
    }
    if (chrNotify->canNotify()) {
      chrNotify->registerForNotify(notifyThunk);
    }
    return true;
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
    bool parsed = false;
    switch (type) {
      case BmsType::Tianpower:
        parsed = tpParseFrame(data, len, bms);
        break;
      case BmsType::Jbd:
        parsed = jbdFeed(proto, data, len, bms);
        break;
      case BmsType::Daly:
        parsed = dalyParse(data, len, bms);
        break;
      case BmsType::Jk:
        parsed = jkFeed(proto, data, len, bms);
        break;
      case BmsType::Ant:
        parsed = antFeed(proto, data, len, bms);
        break;
      default:
        break;
    }
    if (parsed) {
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
    size_t n = 0;
    switch (type) {
      case BmsType::Tianpower:
        for (uint8_t ft : { TIANPOWER_FRAME_SW_VERSION, TIANPOWER_FRAME_HW_VERSION,
                            TIANPOWER_FRAME_STATUS, TIANPOWER_FRAME_GENERAL,
                            TIANPOWER_FRAME_MOSFET, TIANPOWER_FRAME_TEMPS,
                            TIANPOWER_FRAME_CELLS_1_8, TIANPOWER_FRAME_CELLS_9_16 }) {
          tpBuildRequest(ft, buf);
          writeBytes(buf, 4);
          delay(120);
        }
        break;
      case BmsType::Jbd:
        n = jbdBuildCmd(0x03, buf);
        writeBytes(buf, n);
        delay(200);
        n = jbdBuildCmd(0x04, buf);
        writeBytes(buf, n);
        delay(200);
        n = jbdBuildCmd(0x05, buf);
        writeBytes(buf, n);
        break;
      case BmsType::Daly:
        n = dalyBuildRead(0, 62, buf);
        writeBytes(buf, n);
        break;
      case BmsType::Jk:
        n = jkBuildCmd(0x97, buf);
        writeBytes(buf, n);
        delay(400);
        n = jkBuildCmd(0x96, buf);
        writeBytes(buf, n);
        break;
      case BmsType::Ant:
        n = antBuildCmd(0x02, 0x026C, 0x20, buf);
        writeBytes(buf, n);
        delay(300);
        n = antBuildCmd(0x01, 0, 0xBE, buf);
        writeBytes(buf, n);
        break;
      default:
        break;
    }
  }

  void poll() {
    if (!connected || type == BmsType::None) return;
    if (millis() - lastPoll < pollIntervalMs) return;
    lastPoll = millis();

    uint8_t buf[32];
    size_t n = 0;
    switch (type) {
      case BmsType::Tianpower: {
        const uint8_t frames[] = {
          TIANPOWER_FRAME_SW_VERSION, TIANPOWER_FRAME_HW_VERSION, TIANPOWER_FRAME_STATUS,
          TIANPOWER_FRAME_GENERAL, TIANPOWER_FRAME_MOSFET, TIANPOWER_FRAME_TEMPS,
          TIANPOWER_FRAME_CELLS_1_8, TIANPOWER_FRAME_CELLS_9_16
        };
        tpBuildRequest(frames[pollIndex % 8], buf);
        writeBytes(buf, 4);
        pollIndex++;
        pollIntervalMs = 1500;
        break;
      }
      case BmsType::Jbd:
        n = jbdBuildCmd(pollIndex % 2 == 0 ? 0x03 : 0x04, buf);
        writeBytes(buf, n);
        pollIndex++;
        pollIntervalMs = 3000;
        break;
      case BmsType::Daly:
        n = dalyBuildRead(0, 62, buf);
        writeBytes(buf, n);
        pollIntervalMs = 4000;
        break;
      case BmsType::Jk:
        n = jkBuildCmd(0x96, buf);
        writeBytes(buf, n);
        pollIntervalMs = 4000;
        break;
      case BmsType::Ant:
        n = antBuildCmd(0x01, 0, 0xBE, buf);
        writeBytes(buf, n);
        pollIntervalMs = 5000;
        break;
      default:
        break;
    }
  }

  bool connect(BmsType t, const String& devName, const String& devMac, Preferences& prefs) {
    reset();
    if (t == BmsType::None) t = bmsDetectFromName(devName);
    if (t == BmsType::None && tpIsBasenDevice(devName)) t = BmsType::Tianpower;
    if (t == BmsType::None) {
      Serial.println("BMS type unknown — select from scan list");
      return false;
    }

    type = t;
    name = devName;
    mac = devMac;
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

    bool ok = false;
    switch (type) {
      case BmsType::Tianpower:
        ok = getChars(client, TIANPOWER_SERVICE_UUID, TIANPOWER_CHAR_RX_UUID, TIANPOWER_CHAR_TX_UUID);
        break;
      case BmsType::Jbd:
        ok = getChars(client, JBD_SERVICE_UUID, JBD_CHAR_RX_UUID, JBD_CHAR_TX_UUID);
        break;
      case BmsType::Daly:
        ok = getChars(client, DALY_SERVICE_UUID, DALY_CHAR_RX_UUID, DALY_CHAR_TX_UUID);
        break;
      case BmsType::Jk:
      case BmsType::Ant:
        ok = getJkChars(client);
        break;
      default:
        ok = false;
        break;
    }

    if (!ok) {
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
