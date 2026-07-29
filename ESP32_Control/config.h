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
#if defined(ESP32_SLIM_BUILD)
// Classic: gentle recovery — aggressive begin()/BLE was dropping STA in a loop.
#define WIFI_RECONNECT_INTERVAL_MS 30000
#define WIFI_RECONNECT_FAIL_RESTART 15
#define WIFI_SOFT_WAIT_MS 2000
#define WIFI_BEGIN_WAIT_MS 4000
#define WIFI_RECOVERY_ALLOW_SCAN 0
#define WIFI_RECOVERY_NONBLOCK 1
#define WIFI_KICK_GRACE_MS 45000
#define WIFI_DOWN_DEBOUNCE_MS 5000
#define BLE_PAUSE_WIFI_DOWN_MS 500
// Auto-reconnect Basen after WiFi is stable; keep interval gentle for coexistence.
#define BLE_AUTO_RECONNECT 1
#else
#define WIFI_RECONNECT_INTERVAL_MS 15000
#define WIFI_RECONNECT_FAIL_RESTART 5
#define WIFI_SOFT_WAIT_MS 8000
#define WIFI_BEGIN_WAIT_MS 12000
#define WIFI_RECOVERY_ALLOW_SCAN 1
#define WIFI_RECOVERY_NONBLOCK 0
#define WIFI_KICK_GRACE_MS 0
#define WIFI_DOWN_DEBOUNCE_MS 0
#define BLE_PAUSE_WIFI_DOWN_MS 8000
#define BLE_AUTO_RECONNECT 1
#endif

// Default WiFi SSIDs (passwords in wifi_secrets.h — gitignored)
#define WIFI_DEFAULT_SSID_PRIMARY   "mikrotik"
#define WIFI_DEFAULT_SSID_SECONDARY "kalithea"
#define WIFI_DEFAULT_SEED_COUNT 2

#define FIRMWARE_VERSION "3.0.68"

// Shared password for OTA, MQTT remote commands, and protected HTTP APIs
#define DEVICE_CMD_PASSWORD "esp32ota"

#if defined(ESP32_SLIM_BUILD)
// Factory default Basen Green for this Classic hub (user site).
#define BMS_DEFAULT_MAC  "00:9C:17:A9:69:0E"
#define BMS_DEFAULT_NAME "TP_BSTBD-25C-2"
#define BMS_DEFAULT_TYPE "basen"
#else
// Waveshare S3 hub — JK BMS (separate battery from Classic Basen).
#define BMS_DEFAULT_MAC  "C8:47:80:3C:43:28"
#define BMS_DEFAULT_NAME "50514652C101132"
#define BMS_DEFAULT_TYPE "jk"
#endif

#define STATUS_JSON_CAPACITY 12288
#define WIFI_SCAN_JSON_CAPACITY 4096

// Fresh install defaults (factory / first boot)
#define MODBUS_DEFAULT_ENABLED true
#define MODBUS_DEFAULT_PROFILE 0
#define MODBUS_DEFAULT_SLAVE_ID 1
#define MODBUS_DEFAULT_BAUD 9600
#define MODBUS_DEFAULT_METER_ENABLED true
#define MODBUS_DEFAULT_METER_SLAVE_ID 2

#define MQTT_DEFAULT_BROKER "broker.hivemq.com"
#define MQTT_DEFAULT_PORT 1883
#define MQTT_PUBLISH_INTERVAL_MS 2000
#define MQTT_BMS_PUBLISH_INTERVAL_MS 2000

#if defined(ESP32_SLIM_BUILD)
// Classic: short BLE scan — long PREFER_BT scans drop STA.
#define BLE_SCAN_SECONDS 3
#define BLE_SCAN_INTERVAL_MS 160
#define BLE_SCAN_WINDOW_MS 80
// Retry Basen quickly — link flaps under WiFi coexistence.
#define BLE_RECONNECT_MS 12000
#define BLE_POLL_MS 8000
// Wait for STA to settle before any BLE activity after boot/reconnect.
#define BLE_WIFI_STABLE_MS 15000
#define BLE_NOTIFY_KICK_MS 12000
#define BLE_NOTIFY_RESET_MS 90000
#define BLE_CELL_STALE_MS 90000
#else
#define BLE_SCAN_SECONDS 15
#define BLE_SCAN_INTERVAL_MS 80
#define BLE_SCAN_WINDOW_MS 79
#define BLE_RECONNECT_MS 2000
#define BLE_POLL_MS 5000
#define BLE_WIFI_STABLE_MS 0
#define BLE_NOTIFY_KICK_MS 8000
#define BLE_NOTIFY_RESET_MS 30000
#define BLE_CELL_STALE_MS 25000
#endif
#define BLE_SESSION_REFRESH_MS 0
#define BLE_WRITE_RETRY 2

#define MODBUS_POLL_INTERVAL_MS 800
#define MODBUS_READ_TIMEOUT_MS 450
#define MODBUS_WRITE_TIMEOUT_MS 900
