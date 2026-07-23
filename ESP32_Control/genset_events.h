#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>
#include <string.h>

#ifndef GEN_EVENT_MAX
#define GEN_EVENT_MAX 32
#endif

#ifndef GEN_EVENT_NVS_KEY
#define GEN_EVENT_NVS_KEY "gen_events"
#endif

#ifndef GEN_EVENT_NVS_BLOB
#define GEN_EVENT_NVS_BLOB "gen_ev_bin"
#endif

struct GenEvent {
  time_t epoch = 0;
  char local[20] = "";   // YYYY-MM-DD HH:MM:SS
  char type[16] = "";    // start|stop|fault|reset|mode|sched_start|sched_stop|estop
  char detail[56] = "";
  bool ok = true;
};

// Compact NVS record (no UTF-8 local string — rebuilt from epoch on load).
struct GenEventBlob {
  int32_t epoch;
  uint8_t ok;
  char type[14];
  char detail[48];
} __attribute__((packed));

struct GenEventStoreHdr {
  uint8_t magic;    // 'G'
  uint8_t version;  // 1
  uint8_t count;
  uint8_t reserved;
  GenEventBlob items[GEN_EVENT_MAX];
} __attribute__((packed));

struct GenEventLog {
  GenEvent items[GEN_EVENT_MAX];
  uint8_t count = 0;
  uint8_t head = 0;  // next write index (ring)
  Preferences* prefs = nullptr;
  uint16_t lastFaultCode = 0;
  bool faultPrimed = false;
  bool dirty = false;
  unsigned long lastSaveMs = 0;

  void begin(Preferences& p) {
    prefs = &p;
    load();
    Serial.printf("GenEventLog: loaded %u events from NVS\n", (unsigned)count);
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
    dirty = true;
    // Persist immediately so reboot/OTA cannot lose the latest event.
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

  bool loadBlob(Preferences& p) {
    // static: avoid ~2KB stack use from MQTT/poll callbacks
    static GenEventStoreHdr store;
    size_t got = p.getBytesLength(GEN_EVENT_NVS_BLOB);
    if (got < 4 || got > sizeof(store)) return false;
    memset(&store, 0, sizeof(store));
    size_t n = p.getBytes(GEN_EVENT_NVS_BLOB, &store, sizeof(store));
    if (n < 4 || store.magic != 'G' || store.version != 1) return false;
    if (store.count > GEN_EVENT_MAX) store.count = GEN_EVENT_MAX;
    count = 0;
    head = 0;
    for (uint8_t i = 0; i < store.count; i++) {
      const GenEventBlob& b = store.items[i];
      GenEvent& e = items[count];
      e.epoch = (time_t)b.epoch;
      e.ok = b.ok != 0;
      strncpy(e.type, b.type, sizeof(e.type) - 1);
      e.type[sizeof(e.type) - 1] = 0;
      strncpy(e.detail, b.detail, sizeof(e.detail) - 1);
      e.detail[sizeof(e.detail) - 1] = 0;
      formatLocal(e.epoch, e.local, sizeof(e.local));
      count++;
    }
    head = count % GEN_EVENT_MAX;
    return true;
  }

  bool loadLegacyJson(Preferences& p) {
    String raw = p.getString(GEN_EVENT_NVS_KEY, "");
    if (!raw.length() || raw == "[]") return false;
    static StaticJsonDocument<4096> doc;
    doc.clear();
    if (deserializeJson(doc, raw) || !doc.is<JsonArray>()) return false;
    JsonArray arr = doc.as<JsonArray>();
    count = 0;
    head = 0;
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
    return true;
  }

  void load() {
    count = 0;
    head = 0;
    if (!prefs) return;
    if (loadBlob(*prefs)) return;
    if (loadLegacyJson(*prefs)) {
      // Migrate JSON → binary so future saves don't truncate.
      save(*prefs);
      prefs->remove(GEN_EVENT_NVS_KEY);
    }
  }

  bool save(Preferences& p) {
    // static: avoid ~2KB stack use from MQTT/poll callbacks
    static GenEventStoreHdr store;
    memset(&store, 0, sizeof(store));
    store.magic = 'G';
    store.version = 1;
    store.count = count > GEN_EVENT_MAX ? GEN_EVENT_MAX : count;
    for (uint8_t i = 0; i < store.count; i++) {
      uint8_t idx = (head + GEN_EVENT_MAX - count + i) % GEN_EVENT_MAX;
      const GenEvent& e = items[idx];
      GenEventBlob& b = store.items[i];
      b.epoch = (int32_t)e.epoch;
      b.ok = e.ok ? 1 : 0;
      strncpy(b.type, e.type, sizeof(b.type) - 1);
      strncpy(b.detail, e.detail, sizeof(b.detail) - 1);
    }
    size_t wrote = p.putBytes(GEN_EVENT_NVS_BLOB, &store, sizeof(store));
    dirty = false;
    lastSaveMs = millis();
    if (wrote != sizeof(store)) {
      Serial.printf("GenEventLog: NVS save FAILED wrote=%u need=%u\n",
                    (unsigned)wrote, (unsigned)sizeof(store));
      return false;
    }
    return true;
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
