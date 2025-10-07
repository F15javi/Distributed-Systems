from flask import Flask, render_template
from flask_socketio import SocketIO
import paho.mqtt.client as mqtt
import json

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*")

MQTT_BROKER = "test.mosquitto.org"
MQTT_PARENT_TOPIC = "flights/#"  # <-- wildcard for all subtopics

def publish_message(topic, payload):
    mqtt_client.publish(topic, json.dumps(payload))

@socketio.on('warning')
def handle_socket_mqtt(data):
    try:
        aircraftname = data.get('aircraftname')
        message = data.get('message')

        if aircraftname is None:
            print("aircraftname is incorrect")
        if message is None:
            print("meesage is incorrect")

        # Publish to MQTT
        publish_message("flights/"+aircraftname+"/warnings", message)
        print(aircraftname)
        print(f"Published to MQTT: {"flights/"+aircraftname+"/warnings"} -> {message}")
        
    except Exception as e:
        print("Error in handle_socket_mqtt:", e)

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
    #socketio.run(app, host='0.0.0.0', port=5001)
