#!/usr/bin/env python3
"""MQTT START -> wait running -> STOP -> observe countdown."""
import json
import sys
import time

import paho.mqtt.client as mqtt

PW = "esp32ota"
DEV = "1CDBD47A3C50"
CMD = f"home/{DEV}/cmd"
GENSET = f"home/{DEV}/genset"

FIELDS = [
    "genset_state",
    "genset_state_label",
    "running",
    "engine_rpm",
    "remote_start_reg",
    "delay_start_sec",
    "delay_stop_sec",
    "delay_start_active",
    "delay_stop_active",
    "delay_start_remain_sec",
    "delay_stop_remain_sec",
    "last_cmd",
    "last_cmd_ok",
    "last_cmd_detail",
]


def snap(j):
    return {k: j.get(k) for k in FIELDS}


def main():
    log = []
    latest = {}

    def on_msg(_c, _u, m):
        if not m.topic.endswith("/genset"):
            return
        try:
            j = json.loads(m.payload)
            latest.clear()
            latest.update(snap(j))
            log.append((time.time(), dict(latest)))
        except Exception:
            pass

    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    c.on_message = on_msg
    c.connect("broker.hivemq.com", 1883, 60)
    c.subscribe(GENSET)
    c.loop_start()
    time.sleep(3)
    print("BEFORE", latest)
    c.publish(CMD, json.dumps({"password": PW, "genset": "start"}))
    print("sent START")
    for i in range(30):
        time.sleep(2)
        s = latest
        print(
            f"{i * 2:3d}s st={s.get('genset_state')} rpm={s.get('engine_rpm')} "
            f"rs={s.get('remote_start_reg')} dsa={s.get('delay_start_active')} "
            f"dsr={s.get('delay_start_remain_sec')} run={s.get('running')}"
        )
        if s.get("running") and (s.get("engine_rpm") or 0) > 500:
            print("RUNNING - sending STOP")
            c.publish(CMD, json.dumps({"password": PW, "genset": "stop"}))
            break
    else:
        print("never reached running", file=sys.stderr)

    for i in range(25):
        time.sleep(2)
        s = latest
        print(
            f"STOP+{i * 2:3d}s st={s.get('genset_state')} rpm={s.get('engine_rpm')} "
            f"rs={s.get('remote_start_reg')} dsta={s.get('delay_stop_active')} "
            f"dstr={s.get('delay_stop_remain_sec')} cmd={s.get('last_cmd_detail')}"
        )
    c.loop_stop()


if __name__ == "__main__":
    main()
