#pragma once
// Home Assistant REST helper for inverter switches (when Solarman logger is HA-owned).
#include <WiFi.h>
#include <WiFiClient.h>
#include "wifi_secrets.h"

#ifndef HA_BASE_URL
#define HA_BASE_URL ""
#endif
#ifndef HA_LLAT
#define HA_LLAT ""
#endif

inline bool haConfigured() {
  return HA_BASE_URL[0] && HA_LLAT[0];
}

inline bool haParseHostPort(String* hostOut, uint16_t* portOut, String* errOut) {
  String base = HA_BASE_URL;
  if (!base.startsWith("http://")) {
    if (errOut) *errOut = "ha url";
    return false;
  }
  String rest = base.substring(7);
  int slash = rest.indexOf('/');
  if (slash >= 0) rest = rest.substring(0, slash);
  int colon = rest.indexOf(':');
  *hostOut = (colon > 0) ? rest.substring(0, colon) : rest;
  *portOut = 8123;
  if (colon > 0) *portOut = (uint16_t)rest.substring(colon + 1).toInt();
  return hostOut->length() > 0;
}

// Drain remaining HTTP response with a hard deadline (prevents WDT reboot).
inline void haDrain(WiFiClient& cli, unsigned long deadlineMs) {
  while (millis() < deadlineMs && (cli.connected() || cli.available())) {
    while (cli.available()) {
      cli.read();
      if (millis() >= deadlineMs) break;
    }
    if (!cli.available()) delay(1);
    yield();
  }
  cli.stop();
}

inline bool haReadStatusLine(WiFiClient& cli, String* statusOut, unsigned long deadlineMs) {
  String status;
  while (millis() < deadlineMs) {
    if (cli.available()) {
      status = cli.readStringUntil('\n');
      break;
    }
    delay(2);
    yield();
  }
  status.trim();
  if (statusOut) *statusOut = status;
  return status.length() > 0;
}

// entityId e.g. "switch.inverter_off_grid" — returns true if HA accepted the call.
inline bool haSwitchSet(const char* entityId, bool on, String* errOut = nullptr) {
  if (!haConfigured()) {
    if (errOut) *errOut = "ha not configured";
    return false;
  }
  String host;
  uint16_t port = 8123;
  if (!haParseHostPort(&host, &port, errOut)) return false;

  String path = String("/api/services/switch/") + (on ? "turn_on" : "turn_off");
  String body = String("{\"entity_id\":\"") + entityId + "\"}";

  WiFiClient cli;
  cli.setTimeout(2500);
  unsigned long deadline = millis() + 4500;
  if (!cli.connect(host.c_str(), port)) {
    if (errOut) *errOut = "ha connect";
    return false;
  }
  String req;
  req.reserve(512 + body.length());
  req += "POST ";
  req += path;
  req += " HTTP/1.1\r\nHost: ";
  req += host;
  req += "\r\nAuthorization: Bearer ";
  req += HA_LLAT;
  req += "\r\nContent-Type: application/json\r\nContent-Length: ";
  req += String(body.length());
  req += "\r\nConnection: close\r\n\r\n";
  req += body;
  if (cli.print(req) != (int)req.length()) {
    cli.stop();
    if (errOut) *errOut = "ha send";
    return false;
  }

  String status;
  if (!haReadStatusLine(cli, &status, deadline)) {
    cli.stop();
    if (errOut) *errOut = "ha timeout";
    return false;
  }
  haDrain(cli, deadline);
  bool ok = status.indexOf(" 200") > 0 || status.indexOf(" 201") > 0;
  if (!ok && errOut) *errOut = status.length() ? status : "ha bad status";
  return ok;
}

// Read switch.on/off from HA. Returns true if state was parsed.
inline bool haSwitchGet(const char* entityId, bool* onOut, String* errOut = nullptr) {
  if (!haConfigured()) {
    if (errOut) *errOut = "ha not configured";
    return false;
  }
  if (!onOut) return false;
  String host;
  uint16_t port = 8123;
  if (!haParseHostPort(&host, &port, errOut)) return false;

  String path = String("/api/states/") + entityId;
  WiFiClient cli;
  cli.setTimeout(2500);
  unsigned long deadline = millis() + 4500;
  if (!cli.connect(host.c_str(), port)) {
    if (errOut) *errOut = "ha connect";
    return false;
  }
  String req;
  req.reserve(384);
  req += "GET ";
  req += path;
  req += " HTTP/1.1\r\nHost: ";
  req += host;
  req += "\r\nAuthorization: Bearer ";
  req += HA_LLAT;
  req += "\r\nConnection: close\r\n\r\n";
  if (cli.print(req) != (int)req.length()) {
    cli.stop();
    if (errOut) *errOut = "ha send";
    return false;
  }

  String status;
  if (!haReadStatusLine(cli, &status, deadline)) {
    cli.stop();
    if (errOut) *errOut = "ha timeout";
    return false;
  }
  if (status.indexOf(" 200") < 0) {
    haDrain(cli, deadline);
    if (errOut) *errOut = status.length() ? status : "ha bad status";
    return false;
  }

  // Skip headers
  while (millis() < deadline) {
    if (!cli.available() && !cli.connected()) break;
    if (!cli.available()) { delay(1); yield(); continue; }
    String line = cli.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) break;
  }

  String body;
  body.reserve(256);
  while (millis() < deadline && (cli.connected() || cli.available())) {
    while (cli.available()) {
      char c = (char)cli.read();
      if (body.length() < 512) body += c;
      if (millis() >= deadline) break;
    }
    if (!cli.available()) delay(1);
    yield();
  }
  cli.stop();

  int idx = body.indexOf("\"state\"");
  if (idx < 0) {
    if (errOut) *errOut = "ha no state";
    return false;
  }
  int q1 = body.indexOf('"', idx + 7);
  if (q1 < 0) return false;
  int q2 = body.indexOf('"', q1 + 1);
  if (q2 < 0) return false;
  String st = body.substring(q1 + 1, q2);
  if (st == "on") { *onOut = true; return true; }
  if (st == "off") { *onOut = false; return true; }
  if (errOut) *errOut = String("ha state=") + st;
  return false;
}
