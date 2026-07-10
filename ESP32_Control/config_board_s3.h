#pragma once

// Waveshare ESP32-S3-RS485-CAN (Grobotronics) — onboard isolated RS485
#define BOARD_ID "esp32s3-rs485"
#define BOARD_LABEL "Waveshare ESP32-S3-RS485-CAN"

// Screw-terminal GPIO outputs on the module
#define RELAY_COUNT 2
static const int RELAY_PINS[RELAY_COUNT] = { 1, 2 };
static const char* RELAY_LABELS[RELAY_COUNT] = { "GPIO1", "GPIO2" };

// Onboard RS485 UART (DE/RE tied — firmware toggles GPIO21)
#define MODBUS_RX_PIN 18
#define MODBUS_TX_PIN 17
#define MODBUS_DE_PIN 21

// S3 OTA binary on GitHub Pages
#define OTA_FIRMWARE_FILE "firmware-s3.bin"
#define OTA_REMOTE_PATH "/papazachariakis/ESP32/master/docs/firmware-s3.bin"
