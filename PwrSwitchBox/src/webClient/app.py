from flask import Flask, render_template, jsonify
from flask_socketio import SocketIO
import paho.mqtt.client as mqtt
import json

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*")

MQTT_BROKER = "192.168.178.95"
MQTT_TOPIC_SUB = "home/relays"
MQTT_TOPIC_PUB = "home/relays"
MQTT_TOPIC_RELAYSTATEUPDATE_PUB = "home/relaysstate"

# Relais-Zustände als JSON
relays = [
    {"id": 0, "name": "Relay1", "state": "off", "mode": "manual", "timers": []},
    {"id": 1, "name": "Relay2", "state": "off", "mode": "manual", "timers": []},
    {"id": 2, "name": "Relay3", "state": "off", "mode": "manual", "timers": []},
    {"id": 3, "name": "Relay4", "state": "off", "mode": "manual", "timers": []}
]

# MQTT Client einrichten
mqtt_client = mqtt.Client()

def on_connect(client, userdata, flags, rc):
    print("MQTT verbunden!")
    client.subscribe(MQTT_TOPIC_SUB)

def on_message(client, userdata, msg):
    """Empfängt neue Relais-Zustände und aktualisiert die Webseite"""
    global relays
    try:
        new_data = json.loads(msg.payload.decode())  # JSON-String in Python-Dict umwandeln
        relay_id = new_data["relays"][0]['id']
        for relay in relays:
            if relay["id"] == relay_id:
                relays[ relay_id] = new_data["relays"][0]  # Neue Zustände übernehmen
        
        print("Relais-Status aktualisiert:", relays)

        # Aktualisierte Zustände an alle Clients senden
        socketio.emit("update_relays", {"relays": relays})
		
    except json.JSONDecodeError:
        print("Fehler: Ungültiges JSON empfangen!")

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(MQTT_BROKER, 1883, 60)
mqtt_client.loop_start()

@app.route("/")
def index():
    return render_template("index.html", relays=relays)

@socketio.on("set_schedule")
def set_schedule(data):
    relay_id = data["relay_id"]
    timers = data["timers"]

    payload = json.dumps({
        "relay_id": relay_id,
        "timers": timers
    })
    
    
    mqtt_client.publish(MQTT_TOPIC_TIMER, payload)
    print(f"Timer für Relais {relay_id}: {start_time} - {end_time}")

if __name__ == "__main__":
    socketio.run(app, host="0.0.0.0", port=5000, debug=True)
