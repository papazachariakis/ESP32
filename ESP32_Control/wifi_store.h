#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "modbus_rtu.h"
#include "config.h"
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#include <esp_coexist.h>
#endif

#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#define WIFI_SEED_ENABLED 1
#else
#define WIFI_SEED_ENABLED 0
#endif

#ifndef WIFI_STORE_MAX
#define WIFI_STORE_MAX 5
#endif

#ifndef WIFI_STORE_KEY
#define WIFI_STORE_KEY "wifi_list"
#endif

#ifndef WIFI_LAST_SSID_KEY
#define WIFI_LAST_SSID_KEY "wifi_last_ssid"
#endif

inline void wifiStoreRememberSsid(Preferences& prefs, const String& ssid) {
  if (ssid.length() > 0) prefs.putString(WIFI_LAST_SSID_KEY, ssid);
}

inline String wifiStoreCurrentSsid(Preferences& prefs) {
  String ssid = WiFi.SSID();
  ssid.trim();
  if (ssid.length() > 0) return ssid;

  String last = prefs.getString(WIFI_LAST_SSID_KEY, "");
  if (last.length() > 0) return last;

  StaticJsonDocument<1536> doc;
  if (!deserializeJson(doc, prefs.getString(WIFI_STORE_KEY, "[]")) && doc.is<JsonArray>()) {
    for (JsonObject item : doc.as<JsonArray>()) {
      String s = item["ssid"] | "";
      if (s.length() > 0) return s;
    }
  }
  return "";
}

inline void wifiApplyStaTuning() {
  WiFi.setSleep(false);
#if defined(ESP32_SLIM_BUILD)
  // Own reconnect in loop — auto-reconnect races soft begin and can stick the radio.
  WiFi.setAutoReconnect(false);
#else
  WiFi.setAutoReconnect(true);
#endif
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
  // Classic shares one radio with BLE — keep WiFi prioritized except during BLE scan.
  esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
#endif
}

inline bool wifiWaitConnected(uint32_t timeoutMs = 12000) {
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    modbusPump();
    yield();
    delay(50);
  }
  if (WiFi.status() != WL_CONNECTED) return false;
  wifiApplyStaTuning();
  return true;
}

inline void wifiStoreUpsert(Preferences& prefs, const String& ssid, const String& pass) {
  if (ssid.length() == 0) return;

  StaticJsonDocument<1536> doc;
  deserializeJson(doc, prefs.getString(WIFI_STORE_KEY, "[]"));

  String usePass = pass;
  if (usePass.length() == 0 && doc.is<JsonArray>()) {
    for (JsonObject item : doc.as<JsonArray>()) {
      if (String(item["ssid"] | "") == ssid) {
        usePass = item["pass"] | "";
        break;
      }
    }
  }

  StaticJsonDocument<1536> out;
  JsonArray narr = out.to<JsonArray>();
  JsonObject first = narr.add<JsonObject>();
  first["ssid"] = ssid;
  first["pass"] = usePass;

  if (doc.is<JsonArray>()) {
    for (JsonObject item : doc.as<JsonArray>()) {
      if (narr.size() >= WIFI_STORE_MAX) break;
      String s = item["ssid"] | "";
      if (s.length() == 0 || s == ssid) continue;
      JsonObject copy = narr.add<JsonObject>();
      copy["ssid"] = s;
      copy["pass"] = item["pass"] | "";
    }
  }

  String serialized;
  serializeJson(narr, serialized);
  prefs.putString(WIFI_STORE_KEY, serialized);
  Serial.printf("WiFi saved: %s (%u networks)\n", ssid.c_str(), narr.size());
  if (WiFi.status() == WL_CONNECTED) wifiStoreRememberSsid(prefs, ssid);
}

inline size_t wifiStoreAddToJson(Preferences& prefs, JsonArray& outArr) {
  StaticJsonDocument<1536> doc;
  deserializeJson(doc, prefs.getString(WIFI_STORE_KEY, "[]"));
  size_t n = 0;
  if (!doc.is<JsonArray>()) return 0;
  for (JsonObject item : doc.as<JsonArray>()) {
    String ssid = item["ssid"] | "";
    if (ssid.length() == 0) continue;
    outArr.add(ssid);
    n++;
  }
  return n;
}

inline bool wifiStoreGetPass(Preferences& prefs, const String& ssid, String& outPass) {
  StaticJsonDocument<1536> doc;
  deserializeJson(doc, prefs.getString(WIFI_STORE_KEY, "[]"));
  if (!doc.is<JsonArray>()) return false;
  for (JsonObject item : doc.as<JsonArray>()) {
    if (String(item["ssid"] | "") == ssid) {
      outPass = item["pass"] | "";
      return outPass.length() > 0;
    }
  }
  return false;
}

inline bool wifiStoreConnect(Preferences& prefs, const String& ssid, String pass) {
  if (ssid.length() == 0) return false;
  if (pass.length() == 0 && !wifiStoreGetPass(prefs, ssid, pass)) return false;

  wifiStoreUpsert(prefs, ssid, pass);
  WiFi.mode(WIFI_STA);
  wifiApplyStaTuning();
  WiFi.disconnect(true);
  delay(200);
  Serial.printf("WiFi connect: %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());
  bool ok = wifiWaitConnected(WIFI_BEGIN_WAIT_MS + 3000);
  if (ok) wifiStoreRememberSsid(prefs, ssid);
  return ok;
}

// Soft path for runtime drops: last SSID first, no full scan / no erase of STA config.
inline bool wifiStoreTrySoftReconnect(Preferences& prefs) {
  String ssid = wifiStoreCurrentSsid(prefs);
  String pass;
  if (ssid.length() == 0 || !wifiStoreGetPass(prefs, ssid, pass)) return false;

  WiFi.mode(WIFI_STA);
  wifiApplyStaTuning();

  Serial.printf("WiFi soft reconnect: %s\n", ssid.c_str());
  if (WiFi.reconnect() && wifiWaitConnected(WIFI_SOFT_WAIT_MS)) {
    wifiStoreRememberSsid(prefs, ssid);
    return true;
  }

  // begin without wipe — keeps credentials in driver if possible
  WiFi.disconnect(false);
  delay(50);
  Serial.printf("WiFi begin (no scan): %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());
  if (!wifiWaitConnected(WIFI_BEGIN_WAIT_MS)) return false;
  wifiStoreRememberSsid(prefs, ssid);
  return true;
}

// Boot / portal / explicit connect only — sync AP scan blocks the radio for a long time.
inline bool wifiStoreTryConnect(Preferences& prefs) {
  StaticJsonDocument<1536> doc;
  if (deserializeJson(doc, prefs.getString(WIFI_STORE_KEY, "[]"))) return false;
  if (!doc.is<JsonArray>() || doc.size() == 0) return false;

  JsonArray arr = doc.as<JsonArray>();
  WiFi.mode(WIFI_STA);
  wifiApplyStaTuning();
  WiFi.disconnect(true);
  delay(200);

  int found = WiFi.scanNetworks(false, true);
  Serial.printf("WiFi: scan %d APs, %u saved networks\n", found, arr.size());

  int bestSaved = -1;
  int bestRssi = -999;
  for (size_t s = 0; s < arr.size(); s++) {
    String ssid = arr[s]["ssid"] | "";
    for (int i = 0; i < found; i++) {
      if (WiFi.SSID(i) == ssid && WiFi.RSSI(i) > bestRssi) {
        bestRssi = WiFi.RSSI(i);
        bestSaved = (int)s;
      }
    }
  }

  auto tryIdx = [&](int idx) -> bool {
    if (idx < 0 || idx >= (int)arr.size()) return false;
    String ssid = arr[idx]["ssid"] | "";
    String pass = arr[idx]["pass"] | "";
    if (ssid.length() == 0) return false;
    Serial.printf("WiFi connect: %s (RSSI %d)\n", ssid.c_str(), bestRssi);
    WiFi.begin(ssid.c_str(), pass.c_str());
    if (!wifiWaitConnected(WIFI_BEGIN_WAIT_MS)) return false;
    wifiStoreRememberSsid(prefs, ssid);
    return true;
  };

  if (bestSaved >= 0 && tryIdx(bestSaved)) return true;

  for (size_t s = 0; s < arr.size(); s++) {
    if ((int)s == bestSaved) continue;
    WiFi.disconnect(true);
    delay(200);
    if (tryIdx((int)s)) return true;
  }

  return false;
}

inline void wifiStoreClear(Preferences& prefs) {
  prefs.remove(WIFI_STORE_KEY);
  prefs.remove(WIFI_LAST_SSID_KEY);
}

inline void wifiStoreSeedDefaults(Preferences& prefs) {
#if WIFI_SEED_ENABLED
  if (prefs.getBool(WIFI_NO_SEED_KEY, false)) return;
  for (size_t i = 0; i < WIFI_SEED_COUNT; i++) {
    wifiStoreUpsert(prefs, WIFI_SEEDS[i].ssid, WIFI_SEEDS[i].pass);
  }
  Serial.printf("WiFi defaults seeded (%u networks)\n", (unsigned)WIFI_SEED_COUNT);
#endif
}
