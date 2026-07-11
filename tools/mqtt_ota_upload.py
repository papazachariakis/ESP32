#!/usr/bin/env python3
"""Upload docs/firmware.bin to ESP32 via MQTT chunks (no HTTPS required)."""

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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--device", default="000401000000")
    ap.add_argument("--bin", default=os.path.join(os.path.dirname(__file__), "..", "docs", "firmware.bin"))
    ap.add_argument("--broker", default=BROKER)
    ap.add_argument("--port", type=int, default=PORT)
    args = ap.parse_args()

    bin_path = os.path.abspath(args.bin)
    if not os.path.isfile(bin_path):
        print("ERROR: missing", bin_path)
        return 1

    data = open(bin_path, "rb").read()
    size = len(data)
    cmd = f"home/{args.device}/cmd"
    status = f"home/{args.device}/status"
    last_status = {}

    def on_message(client, userdata, msg):
        if msg.topic != status:
            return
        try:
            last_status.clear()
            last_status.update(json.loads(msg.payload.decode("utf-8", "replace")))
        except json.JSONDecodeError:
            pass

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="mqtt-ota-" + str(int(time.time())))
    client.on_message = on_message
    client.connect(args.broker, args.port, 60)
    client.subscribe(status)
    client.loop_start()

    print(f"Waiting for device on {status}...")
    for _ in range(60):
        time.sleep(1)
        if last_status.get("wifi_connected"):
            break
    else:
        print("ERROR: device offline")
        client.loop_stop()
        return 1

    print(f"Uploading {size} bytes via MQTT chunks to {cmd}")
    client.publish(cmd, json.dumps({"ota_mqtt": PASSWORD, "size": size}), qos=1)
    time.sleep(2)

    sent = 0
    seq = 0
    t0 = time.time()
    CHUNK = 384
    BATCH = 4
    while sent < size:
        batch_end = min(sent + CHUNK * BATCH, size)
        off = sent
        while off < batch_end:
            piece = data[off : off + CHUNK]
            b64 = base64.b64encode(piece).decode("ascii")
            payload = json.dumps({"ota_chunk": b64})
            client.publish(cmd, payload, qos=1)
            off += len(piece)
            seq += 1
        sent = batch_end
        pct = sent * 100 // size
        deadline = time.time() + 25
        want_rx = max(0, sent - CHUNK)
        while time.time() < deadline:
            time.sleep(0.3)
            err = last_status.get("ota_error")
            if err:
                print("ERROR during upload:", err)
                client.loop_stop()
                return 1
            rx_s = last_status.get("ota_mqtt_rx", "")
            rx = 0
            if "/" in rx_s:
                try:
                    rx = int(rx_s.split("/", 1)[0])
                except ValueError:
                    pass
            if rx >= want_rx:
                break
        rx_s = last_status.get("ota_mqtt_rx", "?")
        print(f"  {pct}% ({sent}/{size}) device_rx={rx_s}")

    print("Waiting for device to receive all bytes...")
    deadline = time.time() + 60
    while time.time() < deadline:
        time.sleep(0.5)
        err = last_status.get("ota_error")
        if err:
            print("ERROR:", err)
            client.loop_stop()
            return 1
        rx_s = last_status.get("ota_mqtt_rx", "")
        rx = 0
        if "/" in rx_s:
            try:
                rx = int(rx_s.split("/", 1)[0])
            except ValueError:
                pass
        if rx >= size:
            break
    else:
        print("ERROR: timeout waiting for full transfer", last_status.get("ota_mqtt_rx"))
        client.loop_stop()
        return 1

    print("Finalizing...")
    client.publish(cmd, json.dumps({"ota_end": PASSWORD}), qos=1)

    rebooted = False
    gap_before = None
    for i in range(90):
        time.sleep(2)
        phase = last_status.get("ota_phase")
        err = last_status.get("ota_error")
        rx = last_status.get("ota_mqtt_rx")
        if err:
            print("ERROR:", err, "phase=", phase)
            client.loop_stop()
            return 1
        if rx:
            print(f"  device_rx={rx} phase={phase}")
        if phase == "rebooting":
            rebooted = True
        if phase == "mqtt_rx" and i > 5 and not rx:
            print("WARN: no chunk progress on device (old firmware?)")
            client.loop_stop()
            return 1
        if rebooted and last_status.get("wifi_connected") and phase in (None, ""):
            print("SUCCESS: device back online after MQTT OTA")
            print("ip:", last_status.get("ip"))
            client.loop_stop()
            return 0

    print("WARN: finished without clear reboot confirmation")
    print("last status keys:", sorted(last_status.keys()))
    client.loop_stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
