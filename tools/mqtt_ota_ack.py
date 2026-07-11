#!/usr/bin/env python3
"""MQTT OTA for ESP32 v3.0.12 — one chunk at a time, wait for device_rx ack."""

import argparse
import base64
import json
import os
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
CHUNK = 512
GAP_SEC = 6.0


def parse_rx(rx_s):
    if not rx_s:
        return 0
    try:
        return int(str(rx_s).split("/", 1)[0])
    except ValueError:
        return 0


def publish_auth(client, cmd, body):
    payload = {"password": PASSWORD}
    payload.update(body)
    client.publish(cmd, json.dumps(payload), qos=1)


def wait_online(last, seconds=120):
    for _ in range(seconds):
        time.sleep(1)
        if last.get("wifi_connected"):
            return True
    return False


def prep_device(client, cmd, last):
    print("Disable Modbus + BLE for OTA headroom...")
    for _ in range(8):
        publish_auth(client, cmd, {"modbus_cfg": {"enabled": False}})
        publish_auth(client, cmd, {"ble_disconnect": PASSWORD})
        time.sleep(1)
    time.sleep(5)
    print("Device:", last.get("firmware"), last.get("ip"), "genset_enabled=", last.get("genset_enabled"))


def start_ota_session(client, cmd, last, size, attempts=100):
    for attempt in range(attempts):
        publish_auth(client, cmd, {"modbus_cfg": {"enabled": False}})
        time.sleep(0.5)
        publish_auth(client, cmd, {"ota_mqtt": PASSWORD, "size": size})
        for _ in range(20):
            time.sleep(0.5)
            phase = last.get("ota_phase")
            rx_s = str(last.get("ota_mqtt_rx", ""))
            if phase == "mqtt_rx" and str(size) in rx_s:
                print(f"OTA session started (attempt {attempt + 1}): {rx_s}")
                return True
            err = last.get("ota_error")
            if err and "mismatch" not in str(err):
                print("OTA error while starting:", err)
        time.sleep(2)
    return False


def wait_rx(last, target, timeout=90):
    deadline = time.time() + timeout
    while time.time() < deadline:
        rx = parse_rx(last.get("ota_mqtt_rx"))
        err = last.get("ota_error")
        if err and last.get("ota_phase") == "mqtt_rx" and "mismatch" not in str(err):
            return -1
        if rx >= target:
            return rx
        time.sleep(0.5)
    return parse_rx(last.get("ota_mqtt_rx"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--device", default="1CDBD47A3C50")
    ap.add_argument("--bin", default=os.path.join(os.path.dirname(__file__), "..", "docs", "firmware-s3.bin"))
    ap.add_argument("--reboot", action="store_true")
    args = ap.parse_args()

    bin_path = os.path.abspath(args.bin)
    data = open(bin_path, "rb").read()
    size = len(data)
    cmd = f"home/{args.device}/cmd"
    status = f"home/{args.device}/status"
    genset = f"home/{args.device}/genset"
    last = {}

    def on_message(client, userdata, msg):
        try:
            doc = json.loads(msg.payload.decode("utf-8", "replace"))
        except json.JSONDecodeError:
            return
        if msg.topic == status:
            last.clear()
            last.update(doc)
        elif msg.topic == genset:
            last["genset_enabled"] = doc.get("enabled")

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="ack-ota-" + str(int(time.time())))
    client.on_message = on_message
    client.connect(BROKER, PORT, 60)
    client.subscribe(status)
    client.subscribe(genset)
    client.loop_start()
    time.sleep(3)

    if args.reboot:
        print("Rebooting device...")
        publish_auth(client, cmd, {"reboot": PASSWORD})
        time.sleep(15)
        if not wait_online(last):
            print("ERROR: device offline after reboot")
            client.loop_stop()
            return 1

    if not wait_online(last, 90):
        print("ERROR: device offline")
        client.loop_stop()
        return 1

    prep_device(client, cmd, last)

    eta_min = int((size / CHUNK) * GAP_SEC / 60)
    print(f"MQTT OTA: {size} bytes, ~{eta_min} min ({CHUNK} B / {GAP_SEC:.0f}s, ack-wait)")
    if not start_ota_session(client, cmd, last, size):
        print("ERROR: OTA session did not start", {k: last.get(k) for k in ("ota_phase", "ota_error", "ota_mqtt_rx")})
        client.loop_stop()
        return 1

    sent = 0
    seq = 0
    t0 = time.time()
    while sent < size:
        piece = data[sent : sent + CHUNK]
        b64 = base64.b64encode(piece).decode("ascii")
        client.publish(cmd, json.dumps({"ota_chunk": b64}), qos=1)
        sent += len(piece)
        seq += 1

        rx = wait_rx(last, sent, timeout=120)
        if rx < 0:
            print("ERROR during upload:", last.get("ota_error"))
            client.loop_stop()
            return 1
        if rx < sent:
            print(f"WARN chunk {seq}: sent={sent} device_rx={last.get('ota_mqtt_rx')} — retrying modbus off")
            prep_device(client, cmd, last)
            time.sleep(GAP_SEC)
            continue

        if seq % 20 == 0 or sent >= size:
            elapsed = int((time.time() - t0) / 60)
            print(f"  {sent * 100 // size}% sent={sent} device_rx={last.get('ota_mqtt_rx')} ({elapsed} min)")

        time.sleep(GAP_SEC)

    print("Buffer settle (3 min)...")
    time.sleep(180)

    print("Finalizing (ota_end)...")
    publish_auth(client, cmd, {"ota_end": PASSWORD})

    for _ in range(90):
        time.sleep(2)
        err = last.get("ota_error")
        phase = last.get("ota_phase")
        if err and phase != "rebooting" and "mismatch" not in str(err):
            print("ERROR:", err)
            client.loop_stop()
            return 1
        if phase == "rebooting":
            print("Device rebooting...")
            break

    for _ in range(90):
        time.sleep(2)
        fw = last.get("firmware")
        if last.get("wifi_connected") and fw and fw != "3.0.12":
            print("SUCCESS: firmware", fw, "ip", last.get("ip"))
            client.loop_stop()
            return 0

    print("Finished — check device:", {k: last.get(k) for k in ("firmware", "ip", "ota_phase", "ota_error")})
    client.loop_stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
