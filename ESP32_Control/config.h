#pragma once

// Screw terminal outputs (active HIGH for relay modules)
#define RELAY_COUNT 4
static const int RELAY_PINS[RELAY_COUNT] = { 2, 4, 5, 18 };
static const char* RELAY_LABELS[RELAY_COUNT] = { "LED / D2", "Relay D4", "Relay D5", "Relay D18" };

#define WIFI_PORTAL_NAME "ESP32-Setup"
#define WIFI_PORTAL_TIMEOUT_SEC 180
#define WIFI_STORE_MAX 5
#define WIFI_FORCE_PORTAL_KEY "wifi_force_portal"

#define MQTT_DEFAULT_BROKER "broker.hivemq.com"
#define MQTT_DEFAULT_PORT 1883
#define MQTT_PUBLISH_INTERVAL_MS 2000
#define MQTT_BMS_PUBLISH_INTERVAL_MS 2000

#define BLE_SCAN_SECONDS 8
#define BLE_RECONNECT_MS 15000

// RS485 Modbus RTU (MAX485) — Cummins PS0600 (C22D5)
#define MODBUS_RX_PIN 16
#define MODBUS_TX_PIN 17
#define MODBUS_DE_PIN -1
#define MODBUS_POLL_INTERVAL_MS 3000
