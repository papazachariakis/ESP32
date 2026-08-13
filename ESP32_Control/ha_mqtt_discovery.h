#pragma once

// Home Assistant MQTT Discovery for Basen BMS (no PC bridge required).
// HA must use the same MQTT broker as the ESP (default: broker.hivemq.com:1883).

#include <ArduinoJson.h>
#include <PubSubClient.h>

#ifndef HA_DISCOVERY_PREFIX
#define HA_DISCOVERY_PREFIX "homeassistant"
#endif

#ifndef HA_BMS_OBJECT_PREFIX
#define HA_BMS_OBJECT_PREFIX "tp_bstbd_25c_2"
#endif

inline void haDiscoveryPublish(
    PubSubClient& mqtt,
    const char* component,
    const char* objectId,
    JsonDocument& doc) {
  String topic = String(HA_DISCOVERY_PREFIX) + "/" + component + "/" + objectId + "/config";
  char buf[900];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  if (n == 0 || n >= sizeof(buf)) {
    Serial.println("HA discovery payload too large");
    return;
  }
  if (!mqtt.publish(topic.c_str(), buf, true)) {
    Serial.printf("HA discovery publish fail: %s\n", objectId);
  }
  delay(20);
  mqtt.loop();
}

inline void haDiscoveryDevice(JsonObject device, const String& deviceId, const char* fw) {
  JsonArray ids = device.createNestedArray("identifiers");
  ids.add(String("esp32_") + deviceId);
  device["name"] = "ESP32 Classic Basen";
  device["model"] = "ESP32 Classic";
  device["manufacturer"] = "ESP32 Control";
  device["sw_version"] = fw;
}

inline void haDiscoverSensor(
    PubSubClient& mqtt,
    const String& deviceId,
    const String& stateTopic,
    const char* objectId,
    const char* name,
    const char* valueTemplate,
    const char* unit,
    const char* deviceClass,
    const char* stateClass,
    const char* fw) {
  StaticJsonDocument<768> doc;
  doc["name"] = name;
  doc["unique_id"] = objectId;
  doc["object_id"] = objectId;
  doc["state_topic"] = stateTopic;
  doc["value_template"] = valueTemplate;
  if (unit && unit[0]) doc["unit_of_measurement"] = unit;
  if (deviceClass && deviceClass[0]) doc["device_class"] = deviceClass;
  if (stateClass && stateClass[0]) doc["state_class"] = stateClass;
  // Always refresh HA even when numeric value is unchanged (needed for realtime dashboards).
  doc["force_update"] = true;
  haDiscoveryDevice(doc.createNestedObject("device"), deviceId, fw);
  haDiscoveryPublish(mqtt, "sensor", objectId, doc);
}

inline void haDiscoverBinary(
    PubSubClient& mqtt,
    const String& deviceId,
    const String& stateTopic,
    const char* objectId,
    const char* name,
    const char* valueTemplate,
    const char* deviceClass,
    const char* fw) {
  StaticJsonDocument<768> doc;
  doc["name"] = name;
  doc["unique_id"] = objectId;
  doc["object_id"] = objectId;
  doc["state_topic"] = stateTopic;
  doc["value_template"] = valueTemplate;
  doc["payload_on"] = "ON";
  doc["payload_off"] = "OFF";
  if (deviceClass && deviceClass[0]) doc["device_class"] = deviceClass;
  haDiscoveryDevice(doc.createNestedObject("device"), deviceId, fw);
  haDiscoveryPublish(mqtt, "binary_sensor", objectId, doc);
}

inline void publishHaBmsDiscovery(PubSubClient& mqtt, const String& deviceId, const String& bmsTopic, const char* fw) {
  if (!mqtt.connected()) return;
  Serial.println("HA MQTT discovery: publishing Basen sensors...");
  const char* p = HA_BMS_OBJECT_PREFIX;

  {
    char id[64];
    snprintf(id, sizeof(id), "%s_state_of_charge", p);
    haDiscoverSensor(mqtt, deviceId, bmsTopic, id, "State of Charge", "{{ value_json.soc }}", "%", "battery", "measurement", fw);
  }
  {
    char id[64];
    snprintf(id, sizeof(id), "%s_total_voltage", p);
    haDiscoverSensor(mqtt, deviceId, bmsTopic, id, "Total Voltage", "{{ value_json.voltage }}", "V", "voltage", "measurement", fw);
  }
  {
    char id[64];
    snprintf(id, sizeof(id), "%s_current", p);
    haDiscoverSensor(mqtt, deviceId, bmsTopic, id, "Current", "{{ value_json.current }}", "A", "current", "measurement", fw);
  }
  {
    char id[64];
    snprintf(id, sizeof(id), "%s_power", p);
    haDiscoverSensor(mqtt, deviceId, bmsTopic, id, "Power", "{{ value_json.power }}", "W", "power", "measurement", fw);
  }
  {
    char id[64];
    snprintf(id, sizeof(id), "%s_state_of_health", p);
    haDiscoverSensor(mqtt, deviceId, bmsTopic, id, "State of Health", "{{ value_json.soh }}", "%", nullptr, "measurement", fw);
  }
  {
    char id[64];
    snprintf(id, sizeof(id), "%s_temperature", p);
    haDiscoverSensor(mqtt, deviceId, bmsTopic, id, "Temperature", "{{ value_json.avg_temp }}", "\xC2\xB0C", "temperature", "measurement", fw);
  }
  {
    char id[64];
    snprintf(id, sizeof(id), "%s_design_capacity", p);
    haDiscoverSensor(mqtt, deviceId, bmsTopic, id, "Design Capacity", "{{ value_json.capacity_ah }}", "Ah", nullptr, "measurement", fw);
  }
  {
    char id[64];
    snprintf(id, sizeof(id), "%s_capacity_remaining", p);
    haDiscoverSensor(mqtt, deviceId, bmsTopic, id, "Capacity Remaining", "{{ value_json.remaining_ah }}", "Ah", nullptr, "measurement", fw);
  }
  {
    char id[64];
    snprintf(id, sizeof(id), "%s_charge_cycles", p);
    haDiscoverSensor(mqtt, deviceId, bmsTopic, id, "Charge Cycles", "{{ value_json.cycles }}", nullptr, nullptr, "total_increasing", fw);
  }
  {
    char id[64];
    snprintf(id, sizeof(id), "%s_delta_cell_voltage", p);
    haDiscoverSensor(mqtt, deviceId, bmsTopic, id, "Delta Cell Voltage", "{{ value_json.delta_cell_v }}", "V", "voltage", "measurement", fw);
  }
  {
    char id[64];
    snprintf(id, sizeof(id), "%s_min_cell_voltage", p);
    haDiscoverSensor(mqtt, deviceId, bmsTopic, id, "Min Cell Voltage", "{{ value_json.min_cell_v }}", "V", "voltage", "measurement", fw);
  }
  {
    char id[64];
    snprintf(id, sizeof(id), "%s_max_cell_voltage", p);
    haDiscoverSensor(mqtt, deviceId, bmsTopic, id, "Max Cell Voltage", "{{ value_json.max_cell_v }}", "V", "voltage", "measurement", fw);
  }

  for (int i = 0; i < 16; i++) {
    char id[64];
    char name[32];
    char tmpl[40];
    if (i == 0) {
      snprintf(id, sizeof(id), "%s_cell_voltage", p);
      snprintf(name, sizeof(name), "Cell Voltage 1");
    } else {
      snprintf(id, sizeof(id), "%s_cell_voltage_%d", p, i + 1);
      snprintf(name, sizeof(name), "Cell Voltage %d", i + 1);
    }
    snprintf(tmpl, sizeof(tmpl), "{{ value_json.cells[%d] }}", i);
    haDiscoverSensor(mqtt, deviceId, bmsTopic, id, name, tmpl, "V", "voltage", "measurement", fw);
  }

  for (int i = 0; i < 4; i++) {
    char id[64];
    char name[40];
    char tmpl[40];
    if (i == 0) {
      snprintf(id, sizeof(id), "%s_temperature_probe", p);
      snprintf(name, sizeof(name), "Temperature Probe 1");
    } else {
      snprintf(id, sizeof(id), "%s_temperature_probe_%d", p, i + 1);
      snprintf(name, sizeof(name), "Temperature Probe %d", i + 1);
    }
    snprintf(tmpl, sizeof(tmpl), "{{ value_json.temps[%d] }}", i);
    haDiscoverSensor(mqtt, deviceId, bmsTopic, id, name, tmpl, "\xC2\xB0C", "temperature", "measurement", fw);
  }

  {
    char id[64];
    snprintf(id, sizeof(id), "%s_charging", p);
    haDiscoverBinary(mqtt, deviceId, bmsTopic, id, "Charging",
                     "{{ 'ON' if value_json.charging else 'OFF' }}",
                     "battery_charging", fw);
  }
  {
    char id[64];
    snprintf(id, sizeof(id), "%s_discharging", p);
    haDiscoverBinary(mqtt, deviceId, bmsTopic, id, "Discharging",
                     "{{ 'ON' if value_json.discharging else 'OFF' }}",
                     nullptr, fw);
  }

  Serial.println("HA MQTT discovery: done");
}
