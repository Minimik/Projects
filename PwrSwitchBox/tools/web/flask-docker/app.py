from flask import Flask, render_template, jsonify
from flask_socketio import SocketIO
import paho.mqtt.client as mqtt
import json

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*")

MQTT_BROKER = "192.168.178.95"
MQTT_TOPIC_RELAY_SUB = "home/relays"
MQTT_TOPIC_RELAY_PUB = "home/relays"
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
    client.subscribe(MQTT_TOPIC_RELAY_SUB)

def on_message(client, userdata, msg):
    """Empfängt neue Relais-Zustände und aktualisiert die Webseite"""
    global relays
    try:
        new_data = json.loads(msg.payload.decode())  # JSON-String in Python-Dict umwandeln
        relays = new_data["relays"]  # Neue Zustände übernehmen
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

@socketio.on("toggle_relay")
def toggle_relay(data):
    """Wird aufgerufen, wenn ein Benutzer ein Relais über die Webseite umschaltet"""
    relay_id = int(data["relay_id"])
    
    for relay in relays:
        if relay["id"] == relay_id:
            relay["state"] = "on" if relay["state"] == "off" else "off"
            mqtt_client.publish(MQTT_TOPIC_RELAY_PUB, f"{relay_id},{1 if relay['state'] == 'on' else 0}")
    
    # Aktualisierte Relais-Zustände an alle Clients senden
    socketio.emit("update_relays", {"relays": relays})

@socketio.on("request_update")
def request_update():
    """Sendet eine Anfrage an MQTT, um den aktuellen Status abzufragen"""
    print("MQTT-Update angefordert")
    mqtt_client.publish(MQTT_TOPIC_RELAYSTATEUPDATE_PUB, "update")
    
    
if __name__ == "__main__":
    socketio.run(app, host="0.0.0.0", port=5000, debug=True)
