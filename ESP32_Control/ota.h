#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#ifndef ESP32_SLIM_BUILD
#include <WiFiClientSecure.h>
#endif
#include "mbedtls/base64.h"
#include "config.h"

#ifndef OTA_PASSWORD
#define OTA_PASSWORD DEVICE_CMD_PASSWORD
#endif

#ifndef OTA_REMOTE_HOST
#define OTA_REMOTE_HOST "raw.githubusercontent.com"
#endif

#ifndef OTA_REMOTE_PATH
#define OTA_REMOTE_PATH "/papazachariakis/ESP32/master/docs/firmware.bin"
#endif

#ifndef OTA_FIRMWARE_FILE
#define OTA_FIRMWARE_FILE "firmware.bin"
#endif

#ifndef OTA_CDN_PREFIX
#define OTA_CDN_PREFIX "/gh/papazachariakis/ESP32@master/docs/"
#endif

typedef void (*OtaHookFn)();

inline OtaHookFn& otaPrepHook() {
  static OtaHookFn fn = nullptr;
  return fn;
}

inline OtaHookFn& otaPumpHook() {
  static OtaHookFn fn = nullptr;
  return fn;
}

inline OtaHookFn& otaStatusHook() {
  static OtaHookFn fn = nullptr;
  return fn;
}

inline void otaPump() {
  if (otaPumpHook()) otaPumpHook()();
}

inline void otaNotifyStatus() {
  if (otaStatusHook()) otaStatusHook()();
}

inline String& lastOtaError() {
  static String err;
  return err;
}

inline String& otaPhase() {
  static String phase;
  return phase;
}

inline unsigned long& otaErrorAt() {
  static unsigned long ms = 0;
  return ms;
}

inline void otaSetError(const char* phase, const char* msg) {
  otaPhase() = phase;
  lastOtaError() = msg;
  otaErrorAt() = millis();
  Serial.printf("OTA %s: %s\n", phase, msg);
  otaNotifyStatus();
}

inline volatile bool& otaInProgress() {
  static volatile bool busy = false;
  return busy;
}

inline volatile bool& remoteOtaPending() {
  static volatile bool pending = false;
  return pending;
}

inline volatile bool& httpOtaPending() {
  static volatile bool pending = false;
  return pending;
}

inline String& httpOtaUrl() {
  static String url;
  return url;
}

inline volatile bool& mqttOtaActive() {
  static volatile bool active = false;
  return active;
}

inline int& mqttOtaExpected() {
  static int expected = 0;
  return expected;
}

inline int& mqttOtaReceived() {
  static int received = 0;
  return received;
}

inline int& otaDownloadTotal() {
  static int total = 0;
  return total;
}

inline int& otaDownloadReceived() {
  static int received = 0;
  return received;
}

inline void otaResetDownloadProgress() {
  otaDownloadTotal() = 0;
  otaDownloadReceived() = 0;
}

inline bool otaErrorVisible() {
  if (!lastOtaError().length()) return false;
  if (mqttOtaActive() || otaInProgress()) return true;
  return millis() - otaErrorAt() < 120000;
}

inline void requestRemoteOta() {
  otaPhase() = "queued";
  lastOtaError() = "";
  otaResetDownloadProgress();
  remoteOtaPending() = true;
  otaNotifyStatus();
}

inline void requestHttpOta(const char* url) {
  if (!url || !url[0]) return;
  httpOtaUrl() = url;
  otaPhase() = "http_queued";
  lastOtaError() = "";
  httpOtaPending() = true;
  otaNotifyStatus();
}

inline bool remoteOtaPasswordOk(const char* pw) {
  return pw && strcmp(pw, OTA_PASSWORD) == 0;
}

inline uint8_t otaExpectedChipId() {
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_ESP32S3_DEV)
  return 9;  // ESP_CHIP_ID_ESP32S3
#else
  return 0;  // ESP_CHIP_ID_ESP32
#endif
}

inline bool otaValidateFirmwareHeader(const uint8_t* data, size_t len) {
  if (len < 13) return false;
  if (data[0] != 0xE9) return false;
  return data[12] == otaExpectedChipId();
}

inline bool otaSourceAllowsFirmwareFile(const char* pathOrUrl) {
  if (!pathOrUrl || !pathOrUrl[0]) return false;
  return strstr(pathOrUrl, OTA_FIRMWARE_FILE) != nullptr;
}

inline bool otaAbortWrongImage(const char* phase) {
  Update.abort();
  otaInProgress() = false;
  mqttOtaActive() = false;
  mqttOtaExpected() = 0;
  mqttOtaReceived() = 0;
  otaSetError(phase, "wrong firmware image for this board");
  return false;
}

inline void setupArduinoOta() {
  Serial.println("OTA: web /api/ota + MQTT remote/https/http/chunks");
}

inline void handleOtaUpload(WebServer& server) {
  static bool headerChecked = false;
  static uint8_t headerBuf[16];
  static size_t headerLen = 0;
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    headerChecked = false;
    headerLen = 0;
    otaInProgress() = true;
    Serial.printf("Web OTA: %s (expect %s)\n", upload.filename.c_str(), OTA_FIRMWARE_FILE);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
      otaInProgress() = false;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!headerChecked) {
      size_t room = sizeof(headerBuf) - headerLen;
      size_t n = upload.currentSize < room ? upload.currentSize : room;
      if (n > 0) {
        memcpy(headerBuf + headerLen, upload.buf, n);
        headerLen += n;
      }
      if (headerLen >= 13) {
        if (!otaValidateFirmwareHeader(headerBuf, headerLen)) {
          Serial.printf("Web OTA rejected: chip_id=%u expected=%u\n",
                        headerBuf[12], otaExpectedChipId());
          otaAbortWrongImage("upload");
          return;
        }
        headerChecked = true;
      }
    }
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
    headerChecked = false;
    headerLen = 0;
    if (upload.totalSize < 500000 || upload.totalSize > 1966080) {
      Update.abort();
    } else if (Update.end(true)) {
      Serial.printf("Web OTA OK: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    otaInProgress() = false;
    headerChecked = false;
    headerLen = 0;
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

inline bool otaReadHttpHeaders(Client& client, int& contentLen) {
  contentLen = -1;
  bool httpOk = false;
  unsigned long hdrStart = millis();
  while (millis() - hdrStart < 30000) {
    if (!client.available()) {
      if (!client.connected()) break;
      delay(10);
      otaPump();
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
    if (low.startsWith("content-length:")) {
      line = line.substring(15);
      line.trim();
      contentLen = line.toInt();
    }
  }
  return httpOk;
}

inline bool otaDownloadBody(Client& client, int contentLen) {
  const bool unknownSize = (contentLen <= 0);
  if (!unknownSize && (contentLen < 500000 || contentLen > 1966080)) {
    otaSetError("download", "bad firmware size");
    return false;
  }

  uint8_t header[16];
  int hdrRead = 0;
  unsigned long hdrStart = millis();
  while (hdrRead < 16 && millis() - hdrStart < 30000) {
    int avail = client.available();
    if (avail <= 0) {
      if (!client.connected()) break;
      delay(5);
      otaPump();
      continue;
    }
    int n = client.read(header + hdrRead, avail < (16 - hdrRead) ? avail : (16 - hdrRead));
    if (n > 0) hdrRead += n;
  }
  if (hdrRead < 13 || !otaValidateFirmwareHeader(header, hdrRead)) {
    otaSetError("download", "wrong firmware image for this board");
    return false;
  }

  if (!Update.begin(unknownSize ? UPDATE_SIZE_UNKNOWN : (size_t)contentLen)) {
    Update.printError(Serial);
    otaSetError("download", "Update.begin failed");
    return false;
  }
  if (Update.write(header, hdrRead) != (size_t)hdrRead) {
    otaSetError("download", "write failed");
    return false;
  }

  if (!unknownSize) otaDownloadTotal() = contentLen;
  otaDownloadReceived() = hdrRead;
  otaNotifyStatus();

  uint8_t buf[1024];
  int total = hdrRead;
  int lastNotify = hdrRead;
  unsigned long lastData = millis();
  while (millis() - lastData < 180000) {
    if (!unknownSize && total >= contentLen) break;
    int avail = client.available();
    if (avail <= 0) {
      if (!client.connected()) {
        if (unknownSize || total >= contentLen) break;
        delay(10);
        yield();
        continue;
      }
      delay(1);
      otaPump();
      continue;
    }
    lastData = millis();
    int chunk = avail;
    if (chunk > (int)sizeof(buf)) chunk = sizeof(buf);
    if (!unknownSize && chunk > contentLen - total) chunk = contentLen - total;
    int n = client.read(buf, chunk);
    if (n <= 0) break;
    if (Update.write(buf, n) != (size_t)n) {
      Update.printError(Serial);
      otaSetError("download", "write failed");
      return false;
    }
    total += n;
    otaDownloadReceived() = total;
    if (total - lastNotify >= 8192) {
      lastNotify = total;
      Serial.printf("OTA download: %d%s\n", total, unknownSize ? "" : (String(" / ") + contentLen).c_str());
      otaNotifyStatus();
    }
    otaPump();
  }

  otaDownloadReceived() = total;
  otaNotifyStatus();

  if ((!unknownSize && total != contentLen) || !Update.end(true)) {
    Update.printError(Serial);
    otaSetError("download", unknownSize ? "incomplete chunked image" : "incomplete image");
    return false;
  }
  return true;
}

inline void otaPrepareFlash() {
  WiFi.setSleep(false);
  if (otaPrepHook()) otaPrepHook()();
}

inline bool performRemoteOtaFromHost(const char* host, const char* path, bool tls) {
  int contentLen = -1;
#if defined(ESP32_SLIM_BUILD)
  if (tls) {
    otaSetError("https", "disabled on classic slim build");
    return false;
  }
  WiFiClient client;
  client.setTimeout(90000);
  Serial.printf("Remote OTA: http://%s%s\n", host, path);
  otaPhase() = "http_connect";
  otaNotifyStatus();
  if (!client.connect(host, 80)) {
    otaSetError("http", "connect failed");
    return false;
  }
  client.print(String("GET ") + path + " HTTP/1.1\r\nHost: " + host +
               "\r\nUser-Agent: ESP32-Control\r\nConnection: close\r\n\r\n");
  if (!otaReadHttpHeaders(client, contentLen)) {
    otaSetError("http", "HTTP not 200");
    client.stop();
    return false;
  }
  otaPhase() = "http_download";
  otaNotifyStatus();
  if (!otaDownloadBody(client, contentLen)) {
    client.stop();
    return false;
  }
  client.stop();
#else
  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  Client* transport = nullptr;
  if (tls) {
    secureClient.setInsecure();
    secureClient.setTimeout(90000);
    transport = &secureClient;
    Serial.printf("Remote OTA: https://%s%s\n", host, path);
    otaPhase() = "https_connect";
  } else {
    plainClient.setTimeout(90000);
    transport = &plainClient;
    Serial.printf("Remote OTA: http://%s%s\n", host, path);
    otaPhase() = "http_connect";
  }
  otaNotifyStatus();
  if (!transport->connect(host, tls ? 443 : 80)) {
    otaSetError(tls ? "https" : "http", "connect failed");
    return false;
  }
  transport->print(String("GET ") + path + " HTTP/1.1\r\nHost: " + host +
                   "\r\nUser-Agent: ESP32-Control\r\nConnection: close\r\n\r\n");
  if (!otaReadHttpHeaders(*transport, contentLen)) {
    otaSetError(tls ? "https" : "http", "HTTP not 200");
    transport->stop();
    return false;
  }
  otaPhase() = tls ? "https_download" : "http_download";
  otaNotifyStatus();
  if (!otaDownloadBody(*transport, contentLen)) {
    transport->stop();
    return false;
  }
  transport->stop();
#endif

  Serial.printf("Remote OTA OK: %d bytes from %s\n", contentLen, host);
  otaPhase() = "rebooting";
  otaNotifyStatus();
  delay(500);
  ESP.restart();
  return true;
}

inline bool otaParseHttpUrl(const char* url, String& host, String& path, uint16_t& port) {
  if (!url || strncmp(url, "http://", 7) != 0) return false;
  const char* p = url + 7;
  const char* slash = strchr(p, '/');
  String hostPort;
  if (slash) {
    hostPort = String(p, slash - p);
    path = slash;
  } else {
    hostPort = p;
    path = "/";
  }
  int colon = hostPort.indexOf(':');
  if (colon >= 0) {
    host = hostPort.substring(0, colon);
    port = (uint16_t)hostPort.substring(colon + 1).toInt();
    if (port == 0) port = 80;
  } else {
    host = hostPort;
    port = 80;
  }
  return host.length() > 0;
}

inline bool performHttpOta(const char* url) {
  if (!otaSourceAllowsFirmwareFile(url)) {
    otaSetError("http", "URL must point to " OTA_FIRMWARE_FILE);
    return false;
  }
  String host, path;
  uint16_t port = 80;
  if (!otaParseHttpUrl(url, host, path, port)) {
    otaSetError("http", "bad URL");
    return false;
  }
  if (port != 80) {
    otaSetError("http", "only port 80 supported");
    return false;
  }
  return performRemoteOtaFromHost(host.c_str(), path.c_str(), false);
}

inline bool performRemoteOta() {
  if (WiFi.status() != WL_CONNECTED) {
    otaSetError("wifi", "not connected");
    return false;
  }
  otaInProgress() = true;
  otaPrepareFlash();

  Serial.printf("Remote OTA: board expects %s\n", OTA_FIRMWARE_FILE);
#if defined(ESP32_SLIM_BUILD)
  if (performRemoteOtaFromHost(OTA_REMOTE_HOST, OTA_REMOTE_PATH, false)) {
    otaInProgress() = false;
    return true;
  }
#else
  String cdnPath = String(OTA_CDN_PREFIX) + OTA_FIRMWARE_FILE;
  if (performRemoteOtaFromHost("cdn.jsdelivr.net", cdnPath.c_str(), true)) {
    otaInProgress() = false;
    return true;
  }
  if (performRemoteOtaFromHost(OTA_REMOTE_HOST, OTA_REMOTE_PATH, true)) {
    otaInProgress() = false;
    return true;
  }
#endif

  otaInProgress() = false;
  otaSetError("failed", "GitHub/jsDelivr download failed");
  return false;
}

inline bool performPendingHttpOta() {
  if (httpOtaUrl().isEmpty()) return false;
  String url = httpOtaUrl();
  httpOtaUrl() = "";
  otaInProgress() = true;
  otaPrepareFlash();
  bool ok = performHttpOta(url.c_str());
  otaInProgress() = false;
  return ok;
}

inline bool mqttOtaBegin(int size) {
  if (mqttOtaActive()) return false;
  if (size < 500000 || size > 1966080) {
    otaSetError("mqtt", "bad size");
    return false;
  }
  otaPrepareFlash();
  if (!Update.begin(size)) {
    Update.printError(Serial);
    otaSetError("mqtt", "Update.begin failed");
    return false;
  }
  mqttOtaExpected() = size;
  mqttOtaReceived() = 0;
  mqttOtaActive() = true;
  otaInProgress() = true;
  otaPhase() = "mqtt_rx";
  lastOtaError() = "";
  otaNotifyStatus();
  Serial.printf("MQTT OTA begin: %d bytes\n", size);
  return true;
}

inline void mqttOtaAbort(const char* why) {
  if (!mqttOtaActive()) return;
  Update.abort();
  mqttOtaActive() = false;
  mqttOtaExpected() = 0;
  mqttOtaReceived() = 0;
  otaInProgress() = false;
  otaSetError("mqtt", why);
}

inline bool mqttOtaFeedChunk(const uint8_t* data, size_t len) {
  if (!mqttOtaActive() || len == 0) return false;
  if (mqttOtaReceived() == 0) {
    if (len < 13 || !otaValidateFirmwareHeader(data, len)) {
      mqttOtaAbort("wrong firmware image for this board");
      return false;
    }
  }
  if (mqttOtaReceived() + (int)len > mqttOtaExpected()) {
    mqttOtaAbort("overflow");
    return false;
  }
  if (Update.write((uint8_t*)data, len) != len) {
    mqttOtaAbort("write failed");
    return false;
  }
  mqttOtaReceived() += (int)len;
  static int lastPubRx = 0;
  if (mqttOtaReceived() - lastPubRx >= 2048) {
    lastPubRx = mqttOtaReceived();
    otaNotifyStatus();
  }
  return true;
}

inline bool mqttOtaFeedChunkJson(const char* payload, unsigned int length) {
  const char* key = "\"ota_chunk\":\"";
  const char* start = strstr(payload, key);
  if (!start) return false;
  start += strlen(key);
  const char* end = strchr(start, '"');
  if (!end || end <= start) return false;

  size_t b64Len = (size_t)(end - start);
  uint8_t buf[1536];
  size_t outLen = 0;
  int rc = mbedtls_base64_decode(buf, sizeof(buf), &outLen, (const unsigned char*)start, b64Len);
  if (rc != 0 || outLen == 0) {
    mqttOtaAbort("base64 decode failed");
    return false;
  }
  return mqttOtaFeedChunk(buf, outLen);
}

inline bool mqttOtaFinish() {
  if (!mqttOtaActive()) return false;
  if (mqttOtaReceived() != mqttOtaExpected()) {
    char msg[48];
    snprintf(msg, sizeof(msg), "size mismatch %d/%d", mqttOtaReceived(), mqttOtaExpected());
    mqttOtaAbort(msg);
    return false;
  }
  if (!Update.end(true)) {
    Update.printError(Serial);
    mqttOtaAbort("Update.end failed");
    return false;
  }
  mqttOtaActive() = false;
  otaInProgress() = false;
  otaPhase() = "rebooting";
  otaNotifyStatus();
  Serial.printf("MQTT OTA OK: %d bytes\n", mqttOtaReceived());
  delay(500);
  ESP.restart();
  return true;
}

inline const char* otaStatusField() {
  if (otaPhase().isEmpty()) return nullptr;
  return otaPhase().c_str();
}
