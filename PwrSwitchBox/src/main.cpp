// src/main.cpp
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

// WiFi-Zugangsdaten
const char* ssid = "DEIN_WIFI_SSID";
const char* password = "DEIN_WIFI_PASSWORT";

// MQTT-Server
const char* mqttServer = "mqtt.example.com";
const int mqttPort = 1883;
const char* mqttUser = "mqtt_username";
const char* mqttPassword = "mqtt_password";

// Relais-Pins
const int relayPins[] = { D1, D2, D3, D4 };
const int RELAY_COUNT = 4;
const int TIMER_PER_RELAY = 5;

// MQTT-Client und NTP-Client
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000); // Zeitzone +1 Stunde, Aktualisierung alle 60 Sekunden

// Timer-Datenstruktur
// Timer id '-1' means not set; ignore this Timer
struct Timer {
  int id;
  String action;
  String time;
  bool enabled;
  String repeatDays[7];
  String interval;

  time_t parseISO8601(const char* iso8601 /*= NULL*/ );
};

time_t Timer::parseISO8601( const char* iso8601 = NULL )
{
  struct tm t = {};
  if (strptime( (iso8601)? this->time.c_str() : iso8601, "%Y-%m-%dT%H:%M:%S", &t ) )
  {
      return mktime( &t );  // Konvertiere zu time_t
  }
  return 0;  // Fehlerfall
}

// Relais-Datenstruktur
struct Relay {
  int id;
  String name;
  String state;
  String mode;
  Timer timers[TIMER_PER_RELAY]; // Maximal 5 Timer pro Relais
  int timerCount;
};

// Relais-Array
Relay relays[RELAY_COUNT];

// Prototypen
void setupWiFi();
void connectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void parseJSON(const String &jsonString);
void updateRelays();
void processTimers();
bool isTimeToTrigger(const String &targetTime, const String repeatDays[], int repeatDayCount);
String getCurrentDay();

// Hilfsfunktionen
String payloadToString(byte* payload, unsigned int length);
void setupOTA();

void setup() {
  Serial.begin(115200);

  // Relais-Pins initialisieren
  for (int i = 0; i < RELAY_COUNT; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW); // Relais initial aus
  }

  // WLAN verbinden
  setupWiFi();

  // MQTT konfigurieren
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);

  // NTP starten
  timeClient.begin();

  // OTA-Setup
  setupOTA();

  // MQTT verbinden
  connectMQTT();
}

void loop() {
  // MQTT Verbindung prüfen
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();

  // NTP-Zeit synchronisieren
  timeClient.update();

  // OTA aktualisieren
  ArduinoOTA.handle();

  // Timer verarbeiten
  processTimers();
}

int main(  )
{
  setup();

  while ( 1 )
    loop();

}


// WLAN-Verbindung herstellen
void setupWiFi() {
  Serial.print("Verbindung zu WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println(" verbunden!");
}

// OTA konfigurieren
void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnde");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready");
}

// MQTT verbinden
void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Verbinde mit MQTT...");
    if (mqttClient.connect("ESP8266Client", mqttUser, mqttPassword)) {
      Serial.println(" verbunden!");
      mqttClient.subscribe("home/relays"); // MQTT-Topic für Steuerung
    } else {
      Serial.print(" fehlgeschlagen (rc=");
      Serial.print(mqttClient.state());
      Serial.println("). Neuer Versuch in 5 Sekunden...");
      delay(5000);
    }
  }
}

// MQTT-Callback (eingehende Nachrichten)
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = payloadToString(payload, length);
  Serial.print("Nachricht auf ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);

  if (String(topic) == "home/relays") {
    parseJSON(message);
    updateRelays();
  }
}

// JSON-Daten verarbeiten
void parseJSON(const String &jsonString) {
  //DynamicJsonDocument doc(4096);
  JsonDocument doc; //(4096);
  DeserializationError error = deserializeJson( doc, jsonString );

  if (error) {
    Serial.println("JSON-Parsing fehlgeschlagen!");
    return;
  }

  JsonArray relaysArray = doc["relays"].as<JsonArray>();
  for (uint i = 0; i < relaysArray.size() && i < RELAY_COUNT; i++) {
    JsonObject relayObject = relaysArray[i];
    relays[i].id = relayObject["id"];
    relays[i].name = relayObject["name"].as<String>();
    relays[i].state = relayObject["state"].as<String>();
    relays[i].mode = relayObject["mode"].as<String>();
    relays[i].timerCount = 0;

    JsonArray timersArray = relayObject["timers"].as<JsonArray>();
    for (uint j = 0; j < timersArray.size() && j < 5; j++) {
      JsonObject timerObject = timersArray[j];
      relays[i].timers[j].id = timerObject["id"];
      relays[i].timers[j].action = timerObject["action"].as<String>();
      relays[i].timers[j].time = timerObject["time"].as<String>();
      relays[i].timers[j].enabled = timerObject["enabled"];

      JsonArray repeatDaysArray = timerObject["repeat"]["days"].as<JsonArray>();
      for (uint k = 0; k < repeatDaysArray.size() && k < 7; k++) {
        relays[i].timers[j].repeatDays[k] = repeatDaysArray[k].as<String>();
      }
      relays[i].timerCount++;
    }
  }

  updateRelays();
}

// Relais aktualisieren
void updateRelays() {
  for (int i = 0; i < RELAY_COUNT; i++) {
    if (relays[i].mode == "manual") {
      digitalWrite(relayPins[i], relays[i].state == "on" ? HIGH : LOW);
    }
  }
}

String payloadToString(byte* payload, unsigned int length)
{

  String message = "";
  for (unsigned int i = 0; i < length; i++) {
      message += (char)payload[i];
  }

  return message;
}


void processTimers()
{
  for (uint i = 0; i < RELAY_COUNT; i++) {
    if ( relays[i].id != -1 )
    {
      int iTargetRelayState = 0;

      for ( uint t = 0; t < TIMER_PER_RELAY && relays[i].timers[t].id != -1; t++ )
      {
        if ( relays[i].timers[t].enabled )
        {
          time_t tmTimer = relays[i].timers[t].parseISO8601();
          time_t ntpTime = timeClient.getEpochTime();

          if ( tmTimer > ntpTime )
          {
            iTargetRelayState = ( NULL != strcasestr( relays[i].timers[t].action.c_str(), "on" ) );
          }
        }
      }

      digitalWrite( relayPins[ relays[i].id ],iTargetRelayState ); 
      
    //   relays[i].id = relayObject["id"];
    // relays[i].name = relayObject["name"].as<String>();
    // relays[i].state = relayObject["state"].as<String>();
    // relays[i].mode = relayObject["mode"].as<String>();
    // relays[i].timerCount = 0;

    // JsonArray timersArray = relayObject["timers"].as<JsonArray>();
    // for (uint j = 0; j < timersArray.size() && j < 5; j++) {
    //   JsonObject timerObject = timersArray[j];
    //   relays[i].timers[j].id = timerObject["id"];
    //   relays[i].timers[j].action = timerObject["action"].as<String>();
    //   relays[i].timers[j].time = timerObject["time"].as<String>();
    //   relays[i].timers[j].enabled = timerObject["enabled"];

    //   JsonArray repeatDaysArray = timerObject["repeat"]["days"].as<JsonArray>();
    //   for (uint k = 0; k < repeatDaysArray.size() && k < 7; k++) {
    //     relays[i].timers[j].repeatDays[k] = repeatDaysArray[k].as<String>();
    //   }
    //   relays[i].timerCount++;
    }
  }
}