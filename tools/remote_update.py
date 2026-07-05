#!/usr/bin/env python3
"""Try all remote OTA methods in order."""

import json
import os
import subprocess
import sys
import time
import urllib.request

try:
    import paho.mqtt.client as mqtt
except ImportError:
    subprocess.check_call([sys.executable, "-m", "pip", "install", "paho-mqtt", "-q"])
    import paho.mqtt.client as mqtt

DEV = os.environ.get("ESP32_DEVICE", "000401000000")
CMD = f"home/{DEV}/cmd"
STATUS = f"home/{DEV}/status"
st = {}


def on_message(client, userdata, msg):
    if msg.topic != STATUS:
        return
    try:
        st.clear()
        st.update(json.loads(msg.payload.decode("utf-8", "replace")))
    except json.JSONDecodeError:
        pass


def wait_online(client, seconds=60):
    for _ in range(seconds):
        time.sleep(1)
        if st.get("wifi_connected"):
            return True
    return False


def main():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="remote-update")
    client.on_message = on_message
    client.connect("broker.hivemq.com", 1883, 60)
    client.subscribe(STATUS)
    client.loop_start()

    print("Waiting for device...")
    if not wait_online(client):
        print("ERROR: device offline")
        return 1

    print("Device online at", st.get("ip"), "ssid", st.get("wifi_ssid"))
    print("1) HTTPS OTA via GitHub...")
    client.publish(CMD, json.dumps({"ota": "esp32ota"}), qos=1)

    saw_gap = False
    prev = time.time()
    for i in range(36):
        time.sleep(5)
        gap = time.time() - prev
        prev = time.time()
        phase = st.get("ota_phase")
        err = st.get("ota_error")
        print(
            f"  [{i*5:3d}s] gap={gap:.1f}s phase={phase!r} err={err!r} mqtt_rx={st.get('ota_mqtt_rx')}"
        )
        if gap > 12:
            saw_gap = True
        if saw_gap and st.get("wifi_connected") and phase not in ("https_connect", "https_download", "mqtt_rx"):
            print("SUCCESS: reboot detected after HTTPS OTA")
            client.loop_stop()
            return 0
        if err:
            print("HTTPS OTA failed on device:", err)
            break

    if st.get("ota_mqtt_rx") or st.get("ota_phase") == "mqtt_rx":
        print("Device supports MQTT chunk OTA")
    else:
        print("Device firmware lacks MQTT chunk OTA (needs one bootstrap update)")

    print("2) MQTT chunk OTA...")
    client.loop_stop()
    script = os.path.join(os.path.dirname(__file__), "mqtt_ota_upload.py")
    return subprocess.call([sys.executable, script, "--device", DEV])


if __name__ == "__main__":
    raise SystemExit(main())
