#!/usr/bin/env python3
"""Wait for Classic on LAN/MQTT, then HTTP OTA docs/firmware.bin."""
import json
import os
import subprocess
import sys
import time
import urllib.request

import paho.mqtt.client as mqtt

DEV = "38182B8BD5CC"
BIN = os.path.join(os.path.dirname(__file__), "..", "docs", "firmware.bin")
EXPECT = "3.0.55"
PASSWORD = "esp32ota"
WAIT_SEC = 900


def main():
    bin_path = os.path.abspath(BIN)
    last = {}

    def on_m(c, u, m):
        try:
            d = json.loads(m.payload.decode("utf-8", "replace"))
        except Exception:
            return
        if isinstance(d, dict):
            last.clear()
            last.update(d)

    print(f"Waiting Classic online ({WAIT_SEC // 60} min)...", flush=True)
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="c55-" + str(int(time.time())))
    c.on_message = on_m
    c.connect("broker.hivemq.com", 1883, 60)
    c.subscribe(f"home/{DEV}/status")
    c.loop_start()

    ip = None
    deadline = time.time() + WAIT_SEC
    while time.time() < deadline:
        if last.get("wifi_connected") and last.get("ip"):
            ip = last.get("ip")
            print("MQTT ONLINE", last.get("firmware"), ip, last.get("wifi_ssid"), flush=True)
            break
        for cand in ("192.168.99.64", "192.168.99.65", "192.168.1.33"):
            try:
                with urllib.request.urlopen(f"http://{cand}/api/status", timeout=2) as r:
                    j = json.loads(r.read().decode("utf-8", "replace"))
                if j.get("device_id") == DEV or j.get("board") == "esp32":
                    ip = cand
                    print("HTTP ONLINE", j.get("firmware"), ip, flush=True)
                    break
            except Exception:
                pass
        if ip:
            break
        time.sleep(3)
    else:
        print("TIMEOUT still offline", flush=True)
        c.loop_stop()
        return 2

    if last.get("firmware") == EXPECT:
        print("Already on", EXPECT, flush=True)
        c.loop_stop()
        return 0

    c.publish(f"home/{DEV}/cmd", json.dumps({"password": PASSWORD, "ota_abort": True}), qos=1)
    c.publish(f"home/{DEV}/cmd", json.dumps({"password": PASSWORD, "ble_disconnect": True}), qos=1)
    time.sleep(5)
    c.loop_stop()

    print("Web OTA to", ip, flush=True)
    up = os.path.join(os.path.dirname(__file__), "web_ota_upload.py")
    rc = subprocess.call([sys.executable, "-u", up, "--host", ip, "--bin", bin_path])
    print("upload rc", rc, flush=True)

    last.clear()
    c2 = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="v55-" + str(int(time.time())))

    def on2(cli, u, m):
        try:
            d = json.loads(m.payload.decode("utf-8", "replace"))
        except Exception:
            return
        if isinstance(d, dict):
            last.clear()
            last.update(d)

    c2.on_message = on2
    c2.connect("broker.hivemq.com", 1883, 60)
    c2.subscribe(f"home/{DEV}/status")
    c2.loop_start()
    for i in range(120):
        time.sleep(1)
        if last.get("firmware") == EXPECT and last.get("wifi_connected"):
            print("SUCCESS", last.get("firmware"), last.get("ip"), last.get("wifi_ssid"), flush=True)
            c2.loop_stop()
            return 0
        if i % 5 == 0:
            try:
                with urllib.request.urlopen(f"http://{ip}/api/status", timeout=2) as r:
                    j = json.loads(r.read().decode("utf-8", "replace"))
                print("http", j.get("firmware"), flush=True)
                if j.get("firmware") == EXPECT:
                    print("SUCCESS http", j.get("firmware"), flush=True)
                    c2.loop_stop()
                    return 0
            except Exception as e:
                print("wait", i, last.get("firmware"), type(e).__name__, flush=True)
    print("FINAL", last.get("firmware"), last.get("ip"), flush=True)
    c2.loop_stop()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
