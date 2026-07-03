#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#ifndef OTA_PASSWORD
#define OTA_PASSWORD "esp32ota"
#endif

#ifndef OTA_REMOTE_URL
#define OTA_REMOTE_URL "https://raw.githubusercontent.com/papazachariakis/ESP32/master/docs/firmware.bin"
#endif

inline volatile bool& otaInProgress() {
  static volatile bool busy = false;
  return busy;
}

inline void setupArduinoOta() {
  Serial.println("OTA: web /api/ota");
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
