#pragma once

// Classic ESP32 dev board + external XY-485 / TTL-RS485 module
#define BOARD_ID "esp32"
#define BOARD_LABEL "ESP32 Classic · Basen BMS"

// Smaller image for min_spiffs OTA (<= 1.96 MB). Drops HTTPS OTA + ENTES profile.
#define ESP32_SLIM_BUILD 1

#define RELAY_COUNT 4
static const int RELAY_PINS[RELAY_COUNT] = { 2, 4, 5, 18 };
static const char* RELAY_LABELS[RELAY_COUNT] = { "LED / D2", "Relay D4", "Relay D5", "Relay D18" };

// Serial2 → external auto-direction RS485 module (no DE pin)
#define MODBUS_RX_PIN 16
#define MODBUS_TX_PIN 17
#define MODBUS_DE_PIN -1

#define OTA_FIRMWARE_FILE "firmware.bin"
#define OTA_REMOTE_PATH "/papazachariakis/ESP32/master/docs/firmware.bin"
