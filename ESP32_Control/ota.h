#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Update.h>

#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME "esp32"
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD "esp32ota"
#endif

inline volatile bool& otaInProgress() {
  static volatile bool busy = false;
  return busy;
}

inline void setupArduinoOta() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    otaInProgress() = true;
    Serial.println("ArduinoOTA start");
  });
  ArduinoOTA.onEnd([]() {
    otaInProgress() = false;
    Serial.println("ArduinoOTA end");
  });
  ArduinoOTA.onError([](ota_error_t err) {
    otaInProgress() = false;
    Serial.printf("ArduinoOTA error %u\n", err);
  });
  ArduinoOTA.begin();
  Serial.println("ArduinoOTA ready: " + String(OTA_HOSTNAME) + ".local");
}

inline void handleOtaUpload(WebServer& server) {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Web OTA: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Web OTA OK: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

inline void handleOtaDone(WebServer& server) {
  server.sendHeader("Connection", "close");
  bool ok = !Update.hasError();
  server.send(200, "application/json", ok ? "{\"ok\":true,\"msg\":\"Rebooting...\"}" : "{\"ok\":false}");
  if (ok) {
    delay(800);
    ESP.restart();
  }
}

inline void registerOtaRoutes(WebServer& server) {
  server.on(
    "/api/ota", HTTP_POST,
    [&server]() { handleOtaDone(server); },
    [&server]() { handleOtaUpload(server); });
}
