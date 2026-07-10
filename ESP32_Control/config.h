#pragma once

// Board profile: auto-selected from Arduino target, or force S3 with:
//   #define FORCE_BOARD_WAVESHARE_S3_RS485 1
// before including config.h (or uncomment below).
// #define FORCE_BOARD_WAVESHARE_S3_RS485 1

#if defined(FORCE_BOARD_WAVESHARE_S3_RS485) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_ESP32S3_DEV)
#include "config_board_s3.h"
#else
#include "config_board_esp32.h"
#endif

#define WIFI_PORTAL_NAME "ESP32-Setup"
#define WIFI_PORTAL_TIMEOUT_SEC 180
#define WIFI_STORE_MAX 5
#define WIFI_FORCE_PORTAL_KEY "wifi_force_portal"
#define WIFI_NO_SEED_KEY "wifi_no_seed"

// Default WiFi SSIDs (passwords in wifi_secrets.h — gitignored)
#define WIFI_DEFAULT_SSID_PRIMARY   "mikrotik"
#define WIFI_DEFAULT_SSID_SECONDARY "kalithea"
#define WIFI_DEFAULT_SEED_COUNT 2

#define FIRMWARE_VERSION "3.0.7"

// Shared password for OTA, MQTT remote commands, and protected HTTP APIs
#define DEVICE_CMD_PASSWORD "esp32ota"

#define STATUS_JSON_CAPACITY 8192
#define WIFI_SCAN_JSON_CAPACITY 4096

// Fresh install defaults (factory / first boot)
#define MODBUS_DEFAULT_ENABLED true
#define MODBUS_DEFAULT_PROFILE 0
#define MODBUS_DEFAULT_SLAVE_ID 1
#define MODBUS_DEFAULT_BAUD 9600

#define MQTT_DEFAULT_BROKER "broker.hivemq.com"
#define MQTT_DEFAULT_PORT 1883
#define MQTT_PUBLISH_INTERVAL_MS 2000
#define MQTT_BMS_PUBLISH_INTERVAL_MS 2000

#define BLE_SCAN_SECONDS 5
#define BLE_RECONNECT_MS 2000
#define BLE_POLL_MS 5000
#define BLE_NOTIFY_KICK_MS 8000
#define BLE_NOTIFY_RESET_MS 30000
#define BLE_CELL_STALE_MS 25000
#define BLE_SESSION_REFRESH_MS 0
#define BLE_WRITE_RETRY 2

#define MODBUS_POLL_INTERVAL_MS 1500
