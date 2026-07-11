#!/usr/bin/env python3
"""Trigger ESP32 HTTPS firmware pull via MQTT and monitor progress."""

import argparse
import json
import sys
import time

try:
    import paho.mqtt.client as mqtt
except ImportError:
    import subprocess

    subprocess.check_call([sys.executable, "-m", "pip", "install", "paho-mqtt", "-q"])
    import paho.mqtt.client as mqtt

BROKER = "broker.hivemq.com"
PORT = 1883
PASSWORD = "esp32ota"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--device", default="1CDBD47A3C50")
    ap.add_argument("--broker", default=BROKER)
    ap.add_argument("--port", type=int, default=PORT)
    args = ap.parse_args()

    cmd = f"home/{args.device}/cmd"
    status = f"home/{args.device}/status"
    last = {}

    def on_message(client, userdata, msg):
        if msg.topic != status:
            return
        try:
            last.clear()
            last.update(json.loads(msg.payload.decode("utf-8", "replace")))
        except json.JSONDecodeError:
            pass

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="pull-ota-" + str(int(time.time())))
    client.on_message = on_message
    client.connect(args.broker, args.port, 60)
    client.subscribe(status)
    client.loop_start()
    time.sleep(2)

    print("Before:", {k: last.get(k) for k in ("firmware", "ip", "ota_phase", "ota_error")})

    print("Disable Modbus for OTA headroom...")
    client.publish(cmd, json.dumps({"password": PASSWORD, "modbus_cfg": {"enabled": False}}), qos=1)
    time.sleep(4)

    print("Queue remote HTTPS OTA...")
    client.publish(cmd, json.dumps({"password": PASSWORD, "ota": PASSWORD}), qos=1)

    ok_phases = {"queued", "download", "http_queued", "pull", "rebooting", "mqtt_rx"}
    for i in range(180):
        time.sleep(2)
        phase = last.get("ota_phase")
        err = last.get("ota_error")
        fw = last.get("firmware")
        print(f"  {i * 2}s phase={phase} err={err} fw={fw}")
        if err and phase not in ok_phases:
            print("FAILED:", err)
            client.loop_stop()
            return 1
        if phase == "rebooting":
            print("Device rebooting...")
            break
        if fw and fw != "3.0.12":
            print("SUCCESS:", fw)
            client.loop_stop()
            return 0

    for _ in range(60):
        time.sleep(2)
        fw = last.get("firmware")
        if last.get("wifi_connected") and fw and fw != "3.0.12":
            print("SUCCESS online:", fw, "ip", last.get("ip"))
            client.loop_stop()
            return 0

    print("Final:", {k: last.get(k) for k in ("firmware", "ip", "ota_phase", "ota_error")})
    client.loop_stop()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
