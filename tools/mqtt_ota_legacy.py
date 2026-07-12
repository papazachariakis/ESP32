#!/usr/bin/env python3
"""Slow MQTT OTA for ESP32 firmware v3.0.12 (512 B / 5 s pacing)."""

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
GAP_SEC = 5.0


def parse_rx(rx_s):
    if not rx_s:
        return 0
    try:
        return int(str(rx_s).split("/", 1)[0])
    except ValueError:
        return 0


def wait_online(last, seconds=90):
    for _ in range(seconds):
        time.sleep(1)
        if last.get("wifi_connected"):
            return True
    return False


def start_ota_session(client, cmd, last, size, attempts=80):
    payload = json.dumps({"password": PASSWORD, "ota_mqtt": PASSWORD, "size": size})
    for attempt in range(attempts):
        client.publish(cmd, json.dumps({"password": PASSWORD, "modbus_cfg": {"enabled": False}}), qos=1)
        time.sleep(1)
        client.publish(cmd, payload, qos=1)
        for _ in range(10):
            time.sleep(0.5)
            if last.get("ota_phase") == "mqtt_rx" and str(size) in str(last.get("ota_mqtt_rx", "")):
                print(f"OTA session started (attempt {attempt + 1}):", last.get("ota_mqtt_rx"))
                return True
        time.sleep(2)
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--device", default="1CDBD47A3C50")
    ap.add_argument("--bin", default=os.path.join(os.path.dirname(__file__), "..", "docs", "firmware-s3.bin"))
    ap.add_argument("--broker", default=BROKER)
    ap.add_argument("--port", type=int, default=PORT)
    ap.add_argument("--skip-reboot", action="store_true")
    args = ap.parse_args()

    bin_path = os.path.abspath(args.bin)
    if not os.path.isfile(bin_path):
        print("ERROR: missing", bin_path)
        return 1

    data = open(bin_path, "rb").read()
    size = len(data)
    cmd = f"home/{args.device}/cmd"
    status = f"home/{args.device}/status"
    last = {}
    msg_count = 0

    def on_message(client, userdata, msg):
        nonlocal msg_count
        if msg.topic != status:
            return
        msg_count += 1
        try:
            last.clear()
            last.update(json.loads(msg.payload.decode("utf-8", "replace")))
        except json.JSONDecodeError:
            pass

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="legacy-ota-" + str(int(time.time())))
    client.on_message = on_message
    client.connect(args.broker, args.port, 60)
    client.subscribe(status)
    client.loop_start()
    time.sleep(2)

    if not args.skip_reboot:
        print("Reboot x5...")
        reboot_payload = json.dumps({"password": PASSWORD, "reboot": PASSWORD})
        for _ in range(5):
            client.publish(cmd, reboot_payload, qos=1)
            time.sleep(0.2)

        print("Waiting for offline gap...")
        stagnant = 0
        last_count = msg_count
        saw_offline = False
        for _ in range(60):
            time.sleep(1)
            if msg_count == last_count:
                stagnant += 1
            else:
                stagnant = 0
                last_count = msg_count
            if stagnant >= 10:
                saw_offline = True
                break
        print("offline_gap:", saw_offline)

    print("Waiting for device online...")
    if not wait_online(last):
        print("ERROR: device offline")
        client.loop_stop()
        return 1

    print(
        "Device:",
        last.get("firmware"),
        last.get("ip"),
        "ota_error=",
        last.get("ota_error"),
    )

    eta_min = int((size / CHUNK) * GAP_SEC / 60)
    print(f"Starting MQTT OTA: {size} bytes, ~{eta_min} min (512 B every {GAP_SEC:.0f}s)")
    if not start_ota_session(client, cmd, last, size):
        print("ERROR: OTA session did not start", {k: last.get(k) for k in ("ota_phase", "ota_error", "ota_mqtt_rx")})
        client.loop_stop()
        return 1

    sent = 0
    seq = 0
    t0 = time.time()
    while sent < size:
        phase = last.get("ota_phase")
        err = last.get("ota_error")
        if err and phase == "mqtt_rx" and "mismatch" in str(err):
            pass
        elif err and phase == "mqtt_rx":
            print("ERROR during upload:", err)
            client.loop_stop()
            return 1

        piece = data[sent : sent + CHUNK]
        b64 = base64.b64encode(piece).decode("ascii")
        client.publish(cmd, json.dumps({"ota_chunk": b64}, separators=(",", ":")), qos=1)
        sent += len(piece)
        seq += 1

        if seq % 24 == 0 or sent >= size:
            elapsed = int((time.time() - t0) / 60)
            print(
                f"  {sent * 100 // size}% sent={sent} device_rx={last.get('ota_mqtt_rx')} ({elapsed} min)"
            )

        time.sleep(GAP_SEC)

    print("Buffer settle wait (3 min)...")
    time.sleep(180)

    rx = parse_rx(last.get("ota_mqtt_rx"))
    if rx < int(size * 0.9):
        print(f"Extra wait (2 min), device_rx={last.get('ota_mqtt_rx')}")
        time.sleep(120)

    print("Finalizing (ota_end)...")
    client.publish(cmd, json.dumps({"password": PASSWORD, "ota_end": PASSWORD}), qos=1)

    for i in range(90):
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
        if i % 5 == 0:
            print("  wait:", phase, last.get("ota_mqtt_rx"))

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
