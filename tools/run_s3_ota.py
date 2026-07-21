#!/usr/bin/env python3
"""Prep and run ESP HTTPS OTA for S3 device."""

import json
import sys
import time

import paho.mqtt.client as mqtt

DEVICE = "1CDBD47A3C50"
BROKER = "broker.hivemq.com"
PW = "esp32ota"
TARGET = "3.0.42"
CMD = f"home/{DEVICE}/cmd"
STATUS = f"home/{DEVICE}/status"

last = {}


def on_msg(client, userdata, msg):
    if msg.topic != STATUS:
        return
    try:
        last.clear()
        last.update(json.loads(msg.payload.decode("utf-8", "replace")))
    except json.JSONDecodeError:
        pass


def main():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="ota-run-" + str(int(time.time())))
    client.on_message = on_msg
    client.connect(BROKER, 1883, 60)
    client.subscribe(STATUS)
    client.loop_start()
    time.sleep(2)

    def pub(obj):
        client.publish(CMD, json.dumps({"password": PW, **obj}), qos=1)

    print("Start:", {k: last.get(k) for k in ("firmware", "ip", "ota_phase", "ota_error")})

    print("Prep: ota_abort + BLE off + Modbus off...")
    pub({"ota_abort": True})
    time.sleep(1)
    pub({"ble_disconnect": True})
    pub({"modbus_cfg": {"enabled": False}})
    time.sleep(5)

    print("Wait 90s for GitHub CDN...")
    time.sleep(90)

    print("Trigger ESP HTTPS OTA pull...")
    pub({"ota": PW})

    ok_phases = {
        None,
        "queued",
        "https_connect",
        "https_download",
        "http_connect",
        "http_download",
        "rebooting",
    }
    last_rx = ""
    for i in range(300):
        time.sleep(2)
        phase = last.get("ota_phase")
        err = last.get("ota_error")
        rx = last.get("ota_mqtt_rx")
        fw = last.get("firmware")
        wifi = last.get("wifi_connected")
        if rx != last_rx or i % 15 == 0:
            print(f"  {i * 2}s phase={phase} rx={rx} err={err} fw={fw} wifi={wifi}")
            last_rx = rx or last_rx
        if err and phase not in ok_phases:
            print("FAILED:", err, "phase=", phase)
            client.loop_stop()
            return 1
        if phase == "rebooting":
            print("Rebooting...")
            break
        if fw == TARGET:
            print("SUCCESS (during pull):", fw)
            client.loop_stop()
            return 0

    print("Waiting for device online after reboot...")
    for i in range(90):
        time.sleep(2)
        fw = last.get("firmware")
        wifi = last.get("wifi_connected")
        if i % 10 == 0:
            print(f"  wait {i * 2}s fw={fw} wifi={wifi} ip={last.get('ip')}")
        if wifi and fw == TARGET:
            print("SUCCESS:", fw, "ip", last.get("ip"))
            client.loop_stop()
            return 0

    print(
        "Final:",
        {k: last.get(k) for k in ("firmware", "ip", "ota_phase", "ota_error", "ota_mqtt_rx")},
    )
    client.loop_stop()
    return 0 if last.get("firmware") == TARGET else 1


if __name__ == "__main__":
    raise SystemExit(main())
