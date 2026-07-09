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

#define MQTT_DEFAULT_BROKER "broker.hivemq.com"
#define MQTT_DEFAULT_PORT 1883
#define MQTT_PUBLISH_INTERVAL_MS 2000
#define MQTT_BMS_PUBLISH_INTERVAL_MS 2000

#define BLE_SCAN_SECONDS 8
#define BLE_RECONNECT_MS 2000
#define BLE_POLL_MS 3500
#define BLE_NOTIFY_KICK_MS 5000
#define BLE_NOTIFY_RESET_MS 10000
#define BLE_CELL_STALE_MS 8000
#define BLE_SESSION_REFRESH_MS 60000

#define MODBUS_POLL_INTERVAL_MS 3000
