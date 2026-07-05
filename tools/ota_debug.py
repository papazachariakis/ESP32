import json
import time
import paho.mqtt.client as mqtt

CMD = "home/000401000000/cmd"
STATUS = "home/000401000000/status"
seen = []
status_times = []


def on_msg(client, userdata, msg):
    seen.append((msg.topic, msg.payload.decode()))
    if msg.topic == STATUS:
        status_times.append(time.time())


c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="ota-debug")
c.on_message = on_msg
c.connect("broker.hivemq.com", 1883, 60)
c.subscribe(STATUS)
c.subscribe(CMD)
c.loop_start()
time.sleep(4)
print("Publishing OTA to", CMD)
c.publish(CMD, '{"ota":"esp32ota"}', qos=1)

for i in range(120):
    time.sleep(2)
    if i % 15 == 0 and status_times:
        gap = max(status_times) - min(status_times[-10:]) if len(status_times) >= 2 else 0
        print(f"[{i*2}s] status_msgs={len(status_times)} last_gap_batch={gap:.1f}s")

c.loop_stop()
if status_times:
  gaps = [status_times[j]-status_times[j-1] for j in range(1,len(status_times))]
  print("Max status gap:", max(gaps) if gaps else 0)
print("Cmd echoes seen:", sum(1 for t,_ in seen if t==CMD))
