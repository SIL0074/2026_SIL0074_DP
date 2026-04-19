import paho.mqtt.client as mqtt
import json
import time

MQTT_BROKER = "mosquitto"
MQTT_PORT = 1883
MQTT_USER = "YOUR_USERNAME"
MQTT_PASS = "YOUR_PASSWORD"
TOPIC_SUB = "esp-now/infra/#"
TOPIC_PUB_PREFIX = "esp-now/state/charging/"

node_states = {}

def on_connect(client, userdata, flags, rc):
    print(f"Připojeno k MQTT s kódem {rc}")
    client.subscribe(TOPIC_SUB)

def on_message(client, userdata, msg):
    try:
        if msg.topic.startswith("esp-now/state/"):
            return

        payload = json.loads(msg.payload.decode())
        node_id = payload.get("id")
        batt = payload.get("batt")
        
        if node_id is None or batt is None:
            return

        now = time.time()
        if node_id not in node_states:
            node_states[node_id] = {"v": batt, "chg": False, "ts": now}
            return

        state = node_states[node_id]
        old_batt = state["v"]
        old_chg = state["chg"]
        
        new_chg = old_chg
        diff = batt - old_batt
        
        if diff > 0.07:
            new_chg = True
        elif diff < -0.07:
            new_chg = False
            
        node_states[node_id] = {
            "v": batt,
            "chg": new_chg,
            "ts": now
        }
        
        if new_chg != old_chg:
            pub_topic = f"{TOPIC_PUB_PREFIX}{node_id}"
            client.publish(pub_topic, json.dumps({"charging": 1 if new_chg else 0}), retain=True)
            print(f"Uzel {node_id}: Charging={new_chg} (Volt diff: {diff:+.3f})")

    except Exception:
        pass

client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASS)
client.on_connect = on_connect
client.on_message = on_message

print("Charging Detector start...")
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()
