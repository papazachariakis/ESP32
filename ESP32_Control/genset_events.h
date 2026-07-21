#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>

#ifndef GEN_EVENT_MAX
#define GEN_EVENT_MAX 24
#endif

#ifndef GEN_EVENT_NVS_KEY
#define GEN_EVENT_NVS_KEY "gen_events"
#endif

struct GenEvent {
  time_t epoch = 0;
  char local[20] = "";   // YYYY-MM-DD HH:MM:SS
  char type[16] = "";    // start|stop|fault|reset|mode|sched_start|sched_stop|estop
  char detail[56] = "";
  bool ok = true;
};

struct GenEventLog {
  GenEvent items[GEN_EVENT_MAX];
  uint8_t count = 0;
  uint8_t head = 0;  // next write index (ring)
  Preferences* prefs = nullptr;
  uint16_t lastFaultCode = 0;
  bool faultPrimed = false;

  void begin(Preferences& p) {
    prefs = &p;
    load();
  }

  void formatLocal(time_t epoch, char* out, size_t outLen) {
    if (!out || outLen < 20) return;
    out[0] = 0;
    if (epoch <= 0) {
      strncpy(out, "—", outLen - 1);
      out[outLen - 1] = 0;
      return;
    }
    struct tm ti;
    if (!localtime_r(&epoch, &ti)) {
      strncpy(out, "—", outLen - 1);
      out[outLen - 1] = 0;
      return;
    }
    strftime(out, outLen, "%Y-%m-%d %H:%M:%S", &ti);
  }

  void push(const char* type, bool ok, const char* detail) {
    if (!type || !type[0]) return;
    GenEvent& e = items[head];
    e.epoch = time(nullptr);
    if (e.epoch < 1700000000) e.epoch = 0;  // not synced yet
    formatLocal(e.epoch, e.local, sizeof(e.local));
    strncpy(e.type, type, sizeof(e.type) - 1);
    e.type[sizeof(e.type) - 1] = 0;
    e.ok = ok;
    if (detail && detail[0]) {
      strncpy(e.detail, detail, sizeof(e.detail) - 1);
      e.detail[sizeof(e.detail) - 1] = 0;
    } else {
      e.detail[0] = 0;
    }
    head = (head + 1) % GEN_EVENT_MAX;
    if (count < GEN_EVENT_MAX) count++;
    if (prefs) save(*prefs);
  }

  void noteFault(uint16_t faultCode) {
    if (!faultPrimed) {
      lastFaultCode = faultCode;
      faultPrimed = true;
      return;
    }
    if (faultCode == lastFaultCode) return;
    if (faultCode > 0) {
      char buf[40];
      snprintf(buf, sizeof(buf), "Σφάλμα #%u", (unsigned)faultCode);
      push("fault", false, buf);
    } else if (lastFaultCode > 0) {
      char buf[48];
      snprintf(buf, sizeof(buf), "Καθαρισμός #%u", (unsigned)lastFaultCode);
      push("fault_clear", true, buf);
    }
    lastFaultCode = faultCode;
  }

  void load() {
    count = 0;
    head = 0;
    if (!prefs) return;
    String raw = prefs->getString(GEN_EVENT_NVS_KEY, "[]");
    StaticJsonDocument<3072> doc;
    if (deserializeJson(doc, raw) || !doc.is<JsonArray>()) return;
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject o : arr) {
      if (count >= GEN_EVENT_MAX) break;
      GenEvent& e = items[count];
      e.epoch = (time_t)(o["t"] | 0);
      const char* loc = o["local"] | "";
      const char* typ = o["type"] | "";
      const char* det = o["detail"] | "";
      strncpy(e.local, loc, sizeof(e.local) - 1);
      e.local[sizeof(e.local) - 1] = 0;
      strncpy(e.type, typ, sizeof(e.type) - 1);
      e.type[sizeof(e.type) - 1] = 0;
      strncpy(e.detail, det, sizeof(e.detail) - 1);
      e.detail[sizeof(e.detail) - 1] = 0;
      e.ok = o["ok"] | true;
      if (!e.local[0] && e.epoch > 0) formatLocal(e.epoch, e.local, sizeof(e.local));
      count++;
    }
    head = count % GEN_EVENT_MAX;
  }

  void save(Preferences& p) {
    StaticJsonDocument<3072> doc;
    JsonArray arr = doc.to<JsonArray>();
    // Persist oldest→newest so load order matches chronological ring fill
    for (uint8_t i = 0; i < count; i++) {
      uint8_t idx = (head + GEN_EVENT_MAX - count + i) % GEN_EVENT_MAX;
      const GenEvent& e = items[idx];
      JsonObject o = arr.add<JsonObject>();
      o["t"] = (long)e.epoch;
      o["local"] = e.local;
      o["type"] = e.type;
      o["ok"] = e.ok;
      if (e.detail[0]) o["detail"] = e.detail;
    }
    String out;
    serializeJson(doc, out);
    p.putString(GEN_EVENT_NVS_KEY, out);
  }

  void fillJson(JsonObject parent) const {
    JsonArray arr = parent.createNestedArray("events");
    // Newest first for UI
    for (uint8_t i = 0; i < count; i++) {
      uint8_t idx = (head + GEN_EVENT_MAX - 1 - i) % GEN_EVENT_MAX;
      const GenEvent& e = items[idx];
      JsonObject o = arr.add<JsonObject>();
      o["t"] = (long)e.epoch;
      o["local"] = e.local;
      o["type"] = e.type;
      o["ok"] = e.ok;
      if (e.detail[0]) o["detail"] = e.detail;
    }
  }
};

inline GenEventLog& genEvents() {
  static GenEventLog log;
  return log;
}
