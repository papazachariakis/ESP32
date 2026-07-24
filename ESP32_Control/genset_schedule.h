#pragma once

#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>
#include "genset_events.h"

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
  time_t startedAt = 0;
  time_t stopAt = 0;
  uint32_t lastFireDay[GEN_SCHED_SLOTS] = {0};
  unsigned long lastTickMs = 0;
  unsigned long ntpStartedMs = 0;
  unsigned long lastStartRetryMs = 0;
  uint8_t startRetries = 0;

  static uint32_t dateKey(const struct tm& t) {
    return (uint32_t)(t.tm_year + 1900) * 10000u + (uint32_t)(t.tm_mon + 1) * 100u + (uint32_t)t.tm_mday;
  }

  static int dayBitFromTm(const struct tm& t) {
    // tm_wday: 0=Sun … 6=Sat → bit0=Mon … bit6=Sun
    return (t.tm_wday == 0) ? 6 : (t.tm_wday - 1);
  }

  void clearActive(bool clearFireDay) {
    if (clearFireDay && activeSlot >= 0 && activeSlot < GEN_SCHED_SLOTS)
      lastFireDay[activeSlot] = 0;
    activeSlot = -1;
    startedAt = 0;
    stopAt = 0;
    startRetries = 0;
    lastStartRetryMs = 0;
  }

  void sanitizeActiveWindow() {
    if (activeSlot < 0 || activeSlot >= GEN_SCHED_SLOTS) {
      clearActive(false);
      return;
    }
    uint16_t dur = slots[activeSlot].durationMin;
    if (dur < 1) dur = 1;
    if (dur > 600) dur = 600;
    if (startedAt <= 0 && stopAt > 0)
      startedAt = stopAt - (time_t)dur * 60;
    if (startedAt > 0)
      stopAt = startedAt + (time_t)dur * 60;
  }

  void load(Preferences& prefs) {
    masterEnabled = prefs.getBool("gs_en", true);
    for (int i = 0; i < GEN_SCHED_SLOTS; i++) {
      String p = "gs" + String(i) + "_";
      slots[i].enabled = prefs.getBool((p + "on").c_str(), false);
      slots[i].hour = (uint8_t)prefs.getUChar((p + "h").c_str(), (i == 0) ? 7 : 0);
      slots[i].minute = (uint8_t)prefs.getUChar((p + "m").c_str(), 0);
      slots[i].durationMin = prefs.getUShort((p + "dur").c_str(), 30);
      if (slots[i].durationMin < 1) slots[i].durationMin = 1;
      if (slots[i].durationMin > 600) slots[i].durationMin = 600;
      slots[i].days = (uint8_t)prefs.getUChar((p + "d").c_str(), 0x3E);
      lastFireDay[i] = prefs.getUInt((p + "lf").c_str(), 0);
    }
    activeSlot = prefs.getChar("gs_act", -1);
    startedAt = (time_t)prefs.getLong("gs_start", 0);
    stopAt = (time_t)prefs.getLong("gs_stop", 0);
    if (activeSlot < 0 || activeSlot >= GEN_SCHED_SLOTS) {
      clearActive(false);
    } else {
      sanitizeActiveWindow();
    }
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
    prefs.putLong("gs_start", (long)startedAt);
    prefs.putLong("gs_stop", (long)stopAt);
  }

  void fillJson(JsonObject o) const {
    o["master_enabled"] = masterEnabled;
    o["time_synced"] = timeSynced;
    o["active_slot"] = activeSlot;
    if (activeSlot >= 0 && stopAt > 0) {
      o["stop_at_epoch"] = (long)stopAt;
      if (startedAt > 0) o["started_at_epoch"] = (long)startedAt;
      time_t now = time(nullptr);
      if (now > 0 && stopAt > now) o["remaining_sec"] = (long)(stopAt - now);
      else o["remaining_sec"] = 0;
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
    // Any schedule edit cancels a stuck/active run so UI save can unstick the ESP32.
    // Also clears last-fire so the same slot can run again today after a failed start.
    clearActive(true);
    for (int i = 0; i < GEN_SCHED_SLOTS; i++) lastFireDay[i] = 0;
    return true;
  }

  // Greece: EET/EEST with EU DST. Do NOT call configTime(0,0) after setenv —
  // Arduino-ESP32 configTime overwrites TZ with UTC0 and leaves the clock on UTC.
  static constexpr const char* kTzGreece = "EET-2EEST,M3.5.0/3,M10.5.0/4";

  void beginNtp() {
    configTzTime(kTzGreece, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    ntpStartedMs = millis();
  }

  void begin() { beginNtp(); }

  void refreshTimeSynced() {
    if (timeSynced) return;
    struct tm ti;
    if (getLocalTime(&ti, 0) && ti.tm_year + 1900 >= 2024) {
      // Re-apply TZ in case an earlier configTime(0,0) left UTC0.
      setenv("TZ", kTzGreece, 1);
      tzset();
      timeSynced = true;
    }
  }

  void tick(GenManager& genMgr, Preferences& prefs);

  void cancelActive(GenManager& genMgr, Preferences& prefs) {
    if (activeSlot < 0) return;
    genMgr.runGensetCmd("stop");
    clearActive(false);
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
    beginNtp();
  }
  if (!timeSynced) return;

  time_t now = time(nullptr);
  if (now <= 0) return;

  struct tm ti;
  if (!getLocalTime(&ti, 0)) return;

  if (activeSlot >= 0) {
    sanitizeActiveWindow();

    // Expired → STOP
    if (now >= stopAt) {
      Serial.printf("Schedule: slot %d stop after %u min\n", activeSlot + 1,
                    slots[activeSlot].durationMin);
      char slotStop[24];
      snprintf(slotStop, sizeof(slotStop), "slot #%d", activeSlot + 1);
      bool ok = genMgr.runGensetCmd("stop", "sched", slotStop);
      (void)ok;
      clearActive(false);
      save(prefs);
      return;
    }

    // Still within window but engine idle → START did not stick; retry.
    const bool starting = genMgr.startDelayArmed || genMgr.data.delayStartActive
                          || (genMgr.data.gensetState >= 2 && genMgr.data.gensetState <= 7);
    const bool running = genMgr.gensetRunning();
    if (!running && !starting && genMgr.gensetStopped()) {
      if (startRetries < 5 && (lastStartRetryMs == 0 || nowMs - lastStartRetryMs >= 20000)) {
        Serial.printf("Schedule: slot %d retry START (%u)\n", activeSlot + 1, startRetries + 1);
        if (genMgr.runGensetCmd("start", "sched")) {
          startRetries++;
          lastStartRetryMs = nowMs;
          // Keep original stopAt based on first start intent
          if (startedAt <= 0) {
            startedAt = now;
            sanitizeActiveWindow();
          }
          save(prefs);
        } else {
          startRetries++;
          lastStartRetryMs = nowMs;
        }
      } else if (startRetries >= 5) {
        Serial.printf("Schedule: slot %d giving up START retries — clearing active\n", activeSlot + 1);
        clearActive(true);  // allow re-fire later today
        save(prefs);
      }
    }
    return;
  }

  if (!masterEnabled || !genMgr.enabled) return;
  if (!genMgr.remoteAllowsSchedule()) return;

  uint32_t today = dateKey(ti);
  int dayBit = dayBitFromTm(ti);

  for (int i = 0; i < GEN_SCHED_SLOTS; i++) {
    GenScheduleSlot& s = slots[i];
    if (!s.enabled) continue;
    if (!(s.days & (1 << dayBit))) continue;
    if (lastFireDay[i] == today) continue;
    if (ti.tm_hour != s.hour || ti.tm_min != s.minute) continue;
    // Fire anytime within the target minute (lastFireDay prevents double-start)

    Serial.printf("Schedule: slot %d start %02u:%02u for %u min\n", i + 1, s.hour, s.minute,
                  s.durationMin);
    char slotInfo[40];
    snprintf(slotInfo, sizeof(slotInfo), "slot #%d %02u:%02u", i + 1, s.hour, s.minute);
    if (genMgr.runGensetCmd("start", "sched", slotInfo)) {
      activeSlot = i;
      startedAt = now;
      startRetries = 0;
      lastStartRetryMs = nowMs;
      sanitizeActiveWindow();
      lastFireDay[i] = today;
      save(prefs);
    } else {
      // Mark fired attempt so we don't spam every second; UI save clears lastFireDay.
      lastFireDay[i] = today;
      save(prefs);
      Serial.printf("Schedule: slot %d START failed — %s\n", i + 1, genMgr.data.lastError.c_str());
    }
    break;
  }
}
