#pragma once

#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>

struct GenManager;

#define GEN_SCHED_SLOTS 4

struct GenScheduleSlot {
  bool enabled = false;
  uint8_t hour = 7;
  uint8_t minute = 0;
  uint16_t durationMin = 30;
  uint8_t days = 0x3E;  // Mon–Fri (bit0=Mon … bit6=Sun)
};

struct GenSchedule {
  GenScheduleSlot slots[GEN_SCHED_SLOTS];
  bool masterEnabled = true;
  bool timeSynced = false;
  int activeSlot = -1;
  time_t stopAt = 0;
  uint32_t lastFireDay[GEN_SCHED_SLOTS] = {0};
  unsigned long lastTickMs = 0;
  unsigned long ntpStartedMs = 0;

  static uint32_t dateKey(const struct tm& t) {
    return (uint32_t)(t.tm_year + 1900) * 10000u + (uint32_t)(t.tm_mon + 1) * 100u + (uint32_t)t.tm_mday;
  }

  static int dayBitFromTm(const struct tm& t) {
    // tm_wday: 0=Sun … 6=Sat → bit0=Mon … bit6=Sun
    return (t.tm_wday == 0) ? 6 : (t.tm_wday - 1);
  }

  void load(Preferences& prefs) {
    masterEnabled = prefs.getBool("gs_en", true);
    for (int i = 0; i < GEN_SCHED_SLOTS; i++) {
      String p = "gs" + String(i) + "_";
      slots[i].enabled = prefs.getBool((p + "on").c_str(), false);
      slots[i].hour = (uint8_t)prefs.getUChar((p + "h").c_str(), (i == 0) ? 7 : 0);
      slots[i].minute = (uint8_t)prefs.getUChar((p + "m").c_str(), 0);
      slots[i].durationMin = prefs.getUShort((p + "dur").c_str(), 30);
      slots[i].days = (uint8_t)prefs.getUChar((p + "d").c_str(), 0x3E);
      lastFireDay[i] = prefs.getUInt((p + "lf").c_str(), 0);
    }
    activeSlot = prefs.getChar("gs_act", -1);
    stopAt = (time_t)prefs.getLong("gs_stop", 0);
  }

  void save(Preferences& prefs) {
    prefs.putBool("gs_en", masterEnabled);
    for (int i = 0; i < GEN_SCHED_SLOTS; i++) {
      String p = "gs" + String(i) + "_";
      prefs.putBool((p + "on").c_str(), slots[i].enabled);
      prefs.putUChar((p + "h").c_str(), slots[i].hour);
      prefs.putUChar((p + "m").c_str(), slots[i].minute);
      prefs.putUShort((p + "dur").c_str(), slots[i].durationMin);
      prefs.putUChar((p + "d").c_str(), slots[i].days);
      prefs.putUInt((p + "lf").c_str(), lastFireDay[i]);
    }
    prefs.putChar("gs_act", (int8_t)activeSlot);
    prefs.putLong("gs_stop", (long)stopAt);
  }

  void fillJson(JsonObject o) const {
    o["master_enabled"] = masterEnabled;
    o["time_synced"] = timeSynced;
    o["active_slot"] = activeSlot;
    if (activeSlot >= 0 && stopAt > 0) {
      o["stop_at_epoch"] = (long)stopAt;
      time_t now = time(nullptr);
      if (now > 0 && stopAt > now) o["remaining_sec"] = (long)(stopAt - now);
    }
    struct tm ti;
    if (timeSynced && getLocalTime(&ti, 0)) {
      char buf[32];
      strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
      o["local_time"] = buf;
    }
    JsonArray arr = o.createNestedArray("slots");
    for (int i = 0; i < GEN_SCHED_SLOTS; i++) {
      JsonObject s = arr.add<JsonObject>();
      s["enabled"] = slots[i].enabled;
      s["hour"] = slots[i].hour;
      s["minute"] = slots[i].minute;
      s["duration_min"] = slots[i].durationMin;
      s["days"] = slots[i].days;
      s["last_fire_day"] = lastFireDay[i];
    }
  }

  bool applyJson(JsonObject doc) {
    if (doc.containsKey("master_enabled")) masterEnabled = doc["master_enabled"].as<bool>();
    if (doc.containsKey("slots") && doc["slots"].is<JsonArray>()) {
      JsonArray arr = doc["slots"].as<JsonArray>();
      for (size_t i = 0; i < arr.size() && i < (size_t)GEN_SCHED_SLOTS; i++) {
        JsonObject s = arr[i];
        if (s.containsKey("enabled")) slots[i].enabled = s["enabled"].as<bool>();
        if (s.containsKey("hour")) slots[i].hour = (uint8_t)constrain(s["hour"].as<int>(), 0, 23);
        if (s.containsKey("minute")) slots[i].minute = (uint8_t)constrain(s["minute"].as<int>(), 0, 59);
        if (s.containsKey("duration_min"))
          slots[i].durationMin = (uint16_t)constrain(s["duration_min"].as<int>(), 1, 600);
        if (s.containsKey("days")) slots[i].days = (uint8_t)(s["days"].as<int>() & 0x7F);
      }
    }
    return true;
  }

  void begin() {
    // Explicit Greece offset — POSIX TZ alone often leaves ESP32 on UTC in Arduino builds
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
    tzset();
    configTime(7200, 3600, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    ntpStartedMs = millis();
  }

  void refreshTimeSynced() {
    if (timeSynced) return;
    struct tm ti;
    if (getLocalTime(&ti, 0) && ti.tm_year + 1900 >= 2024) timeSynced = true;
  }

  void tick(GenManager& genMgr, Preferences& prefs);

  void cancelActive(GenManager& genMgr, Preferences& prefs) {
    if (activeSlot < 0) return;
    genMgr.runGensetCmd("stop");
    activeSlot = -1;
    stopAt = 0;
    save(prefs);
  }
};

inline void GenSchedule::tick(GenManager& genMgr, Preferences& prefs) {
  if (WiFi.status() != WL_CONNECTED) return;
  unsigned long nowMs = millis();
  if (nowMs - lastTickMs < 1000) return;
  lastTickMs = nowMs;

  refreshTimeSynced();
  if (!timeSynced && ntpStartedMs && nowMs - ntpStartedMs > 120000) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    ntpStartedMs = nowMs;
  }
  if (!timeSynced) return;

  time_t now = time(nullptr);
  if (now <= 0) return;

  struct tm ti;
  if (!getLocalTime(&ti, 0)) return;

  if (activeSlot >= 0) {
    if (now >= stopAt) {
      Serial.printf("Schedule: slot %d stop after %u min\n", activeSlot + 1,
                    slots[activeSlot].durationMin);
      genMgr.runGensetCmd("stop");
      activeSlot = -1;
      stopAt = 0;
      save(prefs);
    }
    return;
  }

  if (!masterEnabled || !genMgr.enabled) return;

  uint32_t today = dateKey(ti);
  int dayBit = dayBitFromTm(ti);

  for (int i = 0; i < GEN_SCHED_SLOTS; i++) {
    GenScheduleSlot& s = slots[i];
    if (!s.enabled) continue;
    if (!(s.days & (1 << dayBit))) continue;
    if (lastFireDay[i] == today) continue;
    if (ti.tm_hour != s.hour || ti.tm_min != s.minute) continue;
    if (ti.tm_sec > 45) continue;

    Serial.printf("Schedule: slot %d start %02u:%02u for %u min\n", i + 1, s.hour, s.minute,
                  s.durationMin);
    if (genMgr.runGensetCmd("start")) {
      activeSlot = i;
      stopAt = now + (time_t)s.durationMin * 60;
      lastFireDay[i] = today;
      save(prefs);
    }
    break;
  }
}
