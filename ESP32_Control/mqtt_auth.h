#pragma once

#include <ArduinoJson.h>
#include "config.h"

inline bool mqttCmdPasswordOk(const char* pw) {
  return pw && pw[0] && strcmp(pw, DEVICE_CMD_PASSWORD) == 0;
}

inline bool mqttDocHasPassword(const JsonDocument& doc) {
  const char* pw = doc["password"] | doc["pw"] | "";
  return mqttCmdPasswordOk(pw);
}

// Accept {"password":"..."} or legacy {"ota":"..."} / {"reboot":"..."} / {"modbus_scan":"..."}.
inline bool mqttDocAuthorized(const JsonDocument& doc) {
  if (mqttDocHasPassword(doc)) return true;
  if (doc.containsKey("ota") && mqttCmdPasswordOk(doc["ota"])) return true;
  if (doc.containsKey("reboot") && mqttCmdPasswordOk(doc["reboot"])) return true;
  if (doc.containsKey("factory_reset") && mqttCmdPasswordOk(doc["factory_reset"])) return true;
  if (doc.containsKey("wifi_clear") && mqttCmdPasswordOk(doc["wifi_clear"])) return true;
  if (doc.containsKey("ota_http") && mqttCmdPasswordOk(doc["ota_http"])) return true;
  if (doc.containsKey("ota_mqtt") && mqttCmdPasswordOk(doc["ota_mqtt"])) return true;
  if (doc.containsKey("ota_end") && mqttCmdPasswordOk(doc["ota_end"])) return true;
  if (doc.containsKey("modbus_scan") && doc["modbus_scan"].is<const char*>())
    return mqttCmdPasswordOk(doc["modbus_scan"]);
  return false;
}

inline bool webJsonPasswordOk(const JsonDocument& doc) {
  const char* pw = doc["password"] | doc["pw"] | "";
  return mqttCmdPasswordOk(pw);
}
