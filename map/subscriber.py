from flask import Flask, render_template
from flask_socketio import SocketIO
import paho.mqtt.client as mqtt
import json

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*")

MQTT_BROKER = "test.mosquitto.org"
MQTT_PARENT_TOPIC = "flights/#"  # <-- wildcard for all subtopics

# --- MQTT Callbacks ---
def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker")
    client.subscribe(MQTT_PARENT_TOPIC)
    print(f"Subscribed to parent topic: {MQTT_PARENT_TOPIC}")

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        topic = msg.topic
        #print(f"Message from {topic}: {data}")

        # Include topic/device info in the message sent to the frontend
        print(f"Message from {topic}: {data}")

        socketio.emit('mqtt_data', data)
    except Exception as e:
        print("Error parsing message:", e)

# --- MQTT Setup ---
mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(MQTT_BROKER, 1883, 60)
mqtt_client.loop_start()

# --- Flask route ---
@app.route('/')
def index():
    return render_template('index.html')

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5000)
