import json
import subprocess
import sys
import time

try:
    import paho.mqtt.client as mqtt
except ImportError:
    subprocess.check_call([sys.executable, "-m", "pip", "install", "paho-mqtt", "-q"])
    import paho.mqtt.client as mqtt

BROKER = "broker.hivemq.com"
PORT = 1883
DEV = "000401000000"
CMD = f"home/{DEV}/cmd"
STATUS = f"home/{DEV}/status"
msgs = []


def on_message(client, userdata, msg):
    msgs.append(msg.payload.decode("utf-8", "replace"))


client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="ota-test-" + str(int(time.time())))
client.on_message = on_message
client.connect(BROKER, PORT, 60)
client.subscribe(STATUS)
client.loop_start()

print("Waiting for device status on", STATUS)
for _ in range(60):
    time.sleep(1)
    if msgs:
        break
else:
    print("ERROR: no status from device after 60s")
    client.loop_stop()
    sys.exit(1)

print("Device online. Publishing OTA command to", CMD)
client.publish(CMD, '{"ota":"esp32ota"}', qos=1)

prev_count = len(msgs)
ota_gap = False
for i in range(50):
    time.sleep(5)
    if len(msgs) == prev_count:
        if i > 2:
            ota_gap = True
            print(f"[{i*5}s] no status (OTA download / reboot?)")
    else:
        prev_count = len(msgs)
        last = msgs[-1]
        try:
            d = json.loads(last)
            b = d.get("bms", {})
            g = d.get("genset", {})
            ip = d.get("ip", "?")
            print(f"[{i*5}s] ip={ip} wifi={d.get('wifi_connected')} bms.valid={b.get('valid')} soc={b.get('soc')} genset={g.get('error','ok')}")
            if ota_gap and d.get("wifi_connected"):
                print("SUCCESS: Remote OTA reboot detected (status resumed after gap)")
            if b.get("valid") and b.get("voltage", 0) > 0:
                print("SUCCESS: BMS parsing OK")
                print(f"  SOC={b.get('soc')}% V={b.get('voltage')} Ah={b.get('remaining_ah')}/{b.get('capacity_ah')}")
                break
        except json.JSONDecodeError:
            print(f"[{i*5}s] non-json status ({len(last)} bytes)")

client.loop_stop()
if msgs:
    print("--- last status excerpt ---")
    print(msgs[-1][:800])
