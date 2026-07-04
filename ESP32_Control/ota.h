#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <WiFiClientSecure.h>

#ifndef OTA_PASSWORD
#define OTA_PASSWORD "esp32ota"
#endif

#ifndef OTA_REMOTE_HOST
#define OTA_REMOTE_HOST "raw.githubusercontent.com"
#endif

#ifndef OTA_REMOTE_PATH
#define OTA_REMOTE_PATH "/papazachariakis/ESP32/master/docs/firmware.bin"
#endif

inline volatile bool& otaInProgress() {
  static volatile bool busy = false;
  return busy;
}

inline volatile bool& remoteOtaPending() {
  static volatile bool pending = false;
  return pending;
}

inline void requestRemoteOta() {
  remoteOtaPending() = true;
}

inline bool remoteOtaPasswordOk(const char* pw) {
  return pw && strcmp(pw, OTA_PASSWORD) == 0;
}

inline void setupArduinoOta() {
  Serial.println("OTA: web /api/ota + MQTT remote");
}

inline void handleOtaUpload(WebServer& server) {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    otaInProgress() = true;
    Serial.printf("Web OTA: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
      otaInProgress() = false;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (upload.totalSize > 1966080) {
      Update.abort();
      otaInProgress() = false;
      return;
    }
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    otaInProgress() = false;
    if (upload.totalSize < 500000 || upload.totalSize > 1966080) {
      Update.abort();
    } else if (Update.end(true)) {
      Serial.printf("Web OTA OK: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    otaInProgress() = false;
    Update.abort();
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

inline bool performRemoteOta() {
  if (WiFi.status() != WL_CONNECTED) return false;
  otaInProgress() = true;
  Serial.println("Remote OTA: HTTPS download...");

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(60000);

  if (!client.connect(OTA_REMOTE_HOST, 443)) {
    Serial.println("Remote OTA: connect failed");
    otaInProgress() = false;
    return false;
  }

  client.print(String("GET ") + OTA_REMOTE_PATH + " HTTP/1.1\r\nHost: " + OTA_REMOTE_HOST +
               "\r\nUser-Agent: ESP32-Control\r\nConnection: close\r\n\r\n");

  int contentLen = -1;
  bool httpOk = false;
  unsigned long hdrStart = millis();
  while (millis() - hdrStart < 20000) {
    if (!client.available()) {
      if (!client.connected()) break;
      delay(10);
      continue;
    }
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) break;
    if (line.startsWith("HTTP/")) {
      httpOk = (line.indexOf(" 200 ") >= 0);
      continue;
    }
    String low = line;
    low.toLowerCase();
    if (low.startsWith("content-length:")) contentLen = line.substring(15).toInt();
  }

  if (!httpOk) {
    Serial.println("Remote OTA: HTTP not 200");
    client.stop();
    otaInProgress() = false;
    return false;
  }

  if (contentLen < 500000 || contentLen > 1966080) {
    Serial.printf("Remote OTA: bad size %d\n", contentLen);
    client.stop();
    otaInProgress() = false;
    return false;
  }

  if (!Update.begin(contentLen)) {
    Update.printError(Serial);
    client.stop();
    otaInProgress() = false;
    return false;
  }

  uint8_t buf[1024];
  int total = 0;
  while (total < contentLen && (client.connected() || client.available())) {
    int avail = client.available();
    if (avail <= 0) {
      delay(1);
      continue;
    }
    int chunk = avail;
    if (chunk > (int)sizeof(buf)) chunk = sizeof(buf);
    if (chunk > contentLen - total) chunk = contentLen - total;
    int n = client.read(buf, chunk);
    if (n <= 0) break;
    if (Update.write(buf, n) != (size_t)n) {
      Update.printError(Serial);
      client.stop();
      otaInProgress() = false;
      return false;
    }
    total += n;
  }
  client.stop();

  if (total != contentLen || !Update.end(true)) {
    Update.printError(Serial);
    otaInProgress() = false;
    return false;
  }

  Serial.printf("Remote OTA OK: %d bytes\n", total);
  otaInProgress() = false;
  delay(500);
  ESP.restart();
  return true;
}
