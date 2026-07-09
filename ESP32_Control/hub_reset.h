#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "wifi_store.h"
#include "config.h"

inline void performFactoryReset(Preferences& prefs) {
  Serial.println("FACTORY RESET: clearing NVS and WiFi...");
  for (int i = 0; i < RELAY_COUNT; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }
  wifiStoreClear(prefs);
  prefs.remove(WIFI_NO_SEED_KEY);
  prefs.remove("ble_mac");
  prefs.remove("ble_name");
  prefs.remove("bms_type");
  prefs.remove("modbus_en");
  prefs.remove("modbus_prof");
  prefs.remove("modbus_id");
  prefs.remove("modbus_baud");
  prefs.remove("modbus_probe");
  prefs.remove("mqtt_broker");
  prefs.remove("mqtt_port");
  for (int i = 0; i < RELAY_COUNT; i++) {
    prefs.remove(("r" + String(i)).c_str());
  }
  prefs.putBool(WIFI_FORCE_PORTAL_KEY, false);

  WiFiManager wm;
  wm.resetSettings();
  WiFi.disconnect(true);
  delay(300);
  ESP.restart();
}

inline void performWifiClear(Preferences& prefs) {
  Serial.println("WiFi CLEAR: removing saved networks...");
  wifiStoreClear(prefs);
  prefs.putBool(WIFI_NO_SEED_KEY, true);
  prefs.putBool(WIFI_FORCE_PORTAL_KEY, true);
  WiFiManager wm;
  wm.resetSettings();
  WiFi.disconnect(true);
  delay(300);
  ESP.restart();
}
