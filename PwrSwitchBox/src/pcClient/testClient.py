import paho.mqtt.client as mqtt

# MQTT-Broker-Details
BROKER = "192.168.178.95"  # Ersetze durch die Adresse deines Brokers
PORT = 1883                   # Standardport für MQTT
SUB_TOPIC = "home/relays"  # Thema, auf das abonniert wird
PUB_TOPIC = "home/relays"    # Thema, auf das publiziert wird

# Callback, wenn die Verbindung zum Broker hergestellt wurde
def on_connect(client, userdata, flags, rc):
    print("Verbunden mit Code: ", rc)
    if rc == 0:
        # Erfolgreich verbunden
        print("Erfolgreich verbunden! Abonniere:", SUB_TOPIC)
        client.subscribe(SUB_TOPIC)
    else:
        print("Verbindung fehlgeschlagen.")

# Callback, wenn eine Nachricht empfangen wird
def on_message(client, userdata, msg):
    print(f"Nachricht empfangen: {msg.payload.decode()} auf Thema: {msg.topic}")

# MQTT-Client erstellen
client = mqtt.Client()

# Callbacks zuweisen
client.on_connect = on_connect
client.on_message = on_message

# Verbindung herstellen
client.connect(BROKER, PORT, 60)

# Publisher-Funktion
def publish_message(client, topic, message):
    client.publish(topic, message)
    print(f"Nachricht '{message}' auf {topic} gesendet.")

# Starte die Client-Schleife in einem eigenen Thread
client.loop_start()

try:
    while True:
        # Beispiel: Nachricht senden
        nachricht = input("Gib eine Nachricht ein, die gesendet werden soll (oder 'exit' zum Beenden): ")
        if nachricht.lower() == 'exit':
            break

        if nachricht == "":
            nachricht = '{"relays":[{"id":3,"name":"Relay1","state":"off","mode":"manual","timers":[]}] }' #{"id":1,"action":"on","time":"2025-01-23T08:00:00","repeat":{"days": ["Monday", "Wednesday", "Friday"],"interval": "weekly"}}
   
        publish_message(client, PUB_TOPIC, nachricht)
except KeyboardInterrupt:
    print("Programm beendet.")
finally:
    client.loop_stop()
    client.disconnect()
