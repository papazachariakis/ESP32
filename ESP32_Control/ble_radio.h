#pragma once

#include <BLEDevice.h>

// ESP32 BLE max is +9 dBm (ESP_PWR_LVL_P9).
#ifndef BLE_TX_POWER_LEVEL
#define BLE_TX_POWER_LEVEL ESP_PWR_LVL_P9
#endif

inline int bleTxPowerDbm(esp_power_level_t level) {
  switch (level) {
    case ESP_PWR_LVL_N12: return -12;
    case ESP_PWR_LVL_N9:  return -9;
    case ESP_PWR_LVL_N6:  return -6;
    case ESP_PWR_LVL_N3:  return -3;
    case ESP_PWR_LVL_N0:  return 0;
    case ESP_PWR_LVL_P3:  return 3;
    case ESP_PWR_LVL_P6:  return 6;
    case ESP_PWR_LVL_P9:  return 9;
    default: return 0;
  }
}

inline void configureBleRadio() {
  const esp_power_level_t pwr = BLE_TX_POWER_LEVEL;
  BLEDevice::setPower(pwr, ESP_BLE_PWR_TYPE_DEFAULT);
  BLEDevice::setPower(pwr, ESP_BLE_PWR_TYPE_SCAN);
  BLEDevice::setPower(pwr, ESP_BLE_PWR_TYPE_ADV);
  // Keep link TX at max while connected to BMS (CONN_HDL0..8).
  for (int h = 0; h <= 8; h++) {
    BLEDevice::setPower(pwr, (esp_ble_power_type_t)h);
  }
  Serial.printf("BLE TX power: +%d dBm (default/scan/adv/conn)\n", bleTxPowerDbm(pwr));
}
