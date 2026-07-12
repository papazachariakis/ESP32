#!/usr/bin/env python3
import json
import time

import paho.mqtt.client as mqtt

DEV = "1CDBD47A3C50"
PW = "esp32ota"
CMD = f"home/{DEV}/cmd"
GENSET = f"home/{DEV}/genset"
STATUS = f"home/{DEV}/status"

g = {}
s = {}


def on_msg(_c, _u, m):
    j = json.loads(m.payload)
    if m.topic.endswith("/genset"):
        g.update(j)
    if m.topic.endswith("/status"):
        s.update(j)


def sg():
    return s.get("genset", g)


def main():
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    c.on_message = on_msg
    c.connect("broker.hivemq.com", 1883, 60)
    c.subscribe(GENSET)
    c.subscribe(STATUS)
    c.loop_start()
    time.sleep(5)

    x = sg()
    print("FW", s.get("firmware"))
    print(
        "modbus enabled", x.get("enabled"), "valid", x.get("valid"),
        "baud", x.get("baud"), "slave", x.get("slave_id"),
    )
    print(
        "state", x.get("genset_state"), x.get("genset_state_label"),
        "rpm", x.get("engine_rpm"), "rs", x.get("remote_start_reg"),
    )
    print(
        "delays start", x.get("delay_start_sec"), "stop", x.get("delay_stop_sec"),
        "tdes", x.get("tdes_sec"), "tdec", x.get("tdec_sec"),
    )
    print("last", x.get("last_cmd"), x.get("last_cmd_ok"), x.get("last_cmd_detail"))
    print("fault", x.get("active_fault"), x.get("fault_type"))

    c.publish(CMD, json.dumps({"password": PW, "genset_delay": {"start_sec": 10, "stop_sec": 30}}))
    time.sleep(8)
    x = sg()
    print("DELAY SAVE", x.get("last_cmd_ok"), x.get("last_cmd_detail"))

    c.publish(CMD, json.dumps({"password": PW, "genset": "start"}))
    for i in range(12):
        time.sleep(3)
        x = sg()
        print(
            f"{i * 3}s cmd={x.get('last_cmd')} ok={x.get('last_cmd_ok')} "
            f"detail={x.get('last_cmd_detail')} st={x.get('genset_state')} "
            f"rpm={x.get('engine_rpm')} rs={x.get('remote_start_reg')}"
        )

    c.loop_stop()


if __name__ == "__main__":
    main()
