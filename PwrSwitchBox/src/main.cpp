// src/main.cpp
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <LittleFS.h>



// WiFi-Zugangsdaten
String ssid = "Discovery Channel";
String password = "466c697069";


// MQTT-Server
const char* mqttServer = "192.168.178.95";
const int mqttPort = 1883;
const char* mqttUser = "mqtt_username";
const char* mqttPassword = "mqtt_password";

#define MQTT_TOPIC_RELAY_SUB "home/relays"
#define MQTT_TOPIC_RELAY_PUB "home/relays"
#define MQTT_TOPIC_RELAYSTATEUPDATE_PUB "home/relaysstate"

const char* jsonTemplate = R"json(
{
  "relays": [
      {
          "id": 0,
          "name": "Relay1",
          "state": "off",
          "mode": "manual",
          "timers": []
      }
  ]
}
)json";
//       ,
//       {
//           "id": 2,
//           "name": "Relay1",
//           "state": "off",
//           "mode": "manual",
//           "timers": []
//       },
//       {
//           "id": 3,
//           "name": "Relay1",
//           "state": "off",
//           "mode": "manual",
//           "timers": []
//       },
//       {
//           "id": 4,
//           "name": "Relay1",
//           "state": "off",
//           "mode": "manual",
//           "timers": []
//       }
//   ]
// }
// )json";


// Relais-Pins
const int relayPins[] = { D1, D2, D3, D4 };
const int RELAY_COUNT = 4;
const int TIMER_PER_RELAY = 5;

#define TRIGGER_PIN 12

// wifimanager can run in a blocking mode or a non blocking mode
// Be sure to know how to process loops with no delay() if using non blocking
bool wm_nonblocking = false; // change to true to use non blocking

WiFiManager wm; // global wm instance
WiFiManagerParameter custom_field; // global param ( for non blocking w params )

// MQTT-Client und NTP-Client
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000); // Zeitzone +1 Stunde, Aktualisierung alle 60 Sekunden


typedef enum enmDayOfWeek {
  monday = 0x01,
  tuesday = 0x02,
  wednesday = 0x04,
  thursday = 0x08,
  friday = 0x10,
  saturday = 0x20,
  sunday = 0x40
} eDoW;


// Timer-Datenstruktur
// Timer id '-1' means not set; ignore this Timer
struct Timer {
  int id;
  byte active;
  short sStarttime;   // the format is Hi-byte means the hour and the Low-byte means the minute e.g. 0D34 means time of Day 13:52
  short sStoptime;    // the format is Hi-byte means the hour and the Low-byte means the minute e.g. 0D34 means time of Day 13:52 
  byte days;          // bit is set which day of the week the time shall be used
  // String repeatDays[7];
  // String interval;

  time_t parseISO8601(const char* iso8601 /*= NULL*/ );
};

time_t Timer::parseISO8601( const char* iso8601 = NULL )
{
  // struct tm t = {};
  // if (strptime( (iso8601)? this->time.c_str() : iso8601, "%Y-%m-%dT%H:%M:%S", &t ) )
  // {
  //     return mktime( &t );  // Konvertiere zu time_t
  // }
  return 0;  // Fehlerfall
}

// Relais-Datenstruktur
struct Relay {
  int id;
  String name;
  String state;
  String mode;
  Timer timers[TIMER_PER_RELAY]; // Maximal 5 Timer pro Relais
};

// Relais-Array
Relay relays[RELAY_COUNT];

// Prototypen
void setupWiFi();
void setupWifiManager();
void connectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void parseJSON(const String &jsonString);
void updateRelays();
void processTimers(  uint8_t nmbRelay );
//bool isTimeToTrigger(const String &targetTime, const String repeatDays[], int repeatDayCount);
//String getCurrentDay();
void prepareJSON( void );

// Hilfsfunktionen
String payloadToString(byte* payload, unsigned int length);
void setupOTA();


void restoreRelayConfigFromFlash()
{
  File file = LittleFS.open("/config.bin", "rb");
  if (!file) {
    Serial.println("Fehler beim Öffnen der Datei!");
    return;
  }

  for (int i = 0; i < RELAY_COUNT; i++) {

    file.read((uint8_t*)&relays, sizeof(relays));  // Binär lesen

    pinMode( relayPins[i], OUTPUT );
    digitalWrite( relayPins[i], LOW ); // Relais initial aus
  }

  file.close();
}

void setup() {

  Serial.begin(115200);

//  setupWifiManager( );

  // Relais-Pins initialisieren
  for (int i = 0; i < RELAY_COUNT; i++) {

    relays[i].id = i;
    relays[i].state = "on";
    relays[i].name = String("Relay") + String(i+1);
    relays[i].mode = "manual";

    for ( int j = 0; j < TIMER_PER_RELAY; j++ )
    {
      memset( &(relays[i].timers[j]) , 0, sizeof(Timer) );
    }


    pinMode( relayPins[i], OUTPUT );
    for ( int j = 0; j < TIMER_PER_RELAY; j++ )
    {
      memset( &(relays[i].timers[j]) , 0, sizeof(Timer) );
    }

    digitalWrite( relayPins[i], LOW ); // Relais initial aus
  }

  // WLAN verbinden
  setupWiFi();

  // MQTT konfigurieren
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);

  // NTP starten
  timeClient.begin();
  // timeClient.getDay();

  // OTA-Setup
  setupOTA();

  // 1. LittleFS starten
  if (!LittleFS.begin()) {
    Serial.println("Fehler beim Mounten von LittleFS!");
    return;
  }

  // MQTT verbinden
  connectMQTT();

  // Relais-Pins initialisieren
  for (int i = 0; i < RELAY_COUNT; i++) {
    digitalWrite( relayPins[i], HIGH ); // Relais initial aus
  }
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
  //processTimers();

  //updateRelays();
  //  digitalWrite( relayPins[3], LOW );
  //  delay( 1000 );
  //  digitalWrite( relayPins[3], HIGH );
  //  delay( 1000 );
}

// int main(  )
// {
//   setup();

// while ( 1 )
//   loop();

// }


// WLAN-Verbindung herstellen
void setupWiFi() {
  WiFi.disconnect();

  Serial.print("Verbindung zu WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println( WiFi.localIP().toString() );											  
  Serial.println(" verbunden!");
  Serial.println("IP-Adresse: " + WiFi.localIP().toString());
}

String getParam(String name){
  //read parameter from server, for customhmtl input
  String value;
  if(wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }

  return value;
}

void saveParamCallback(){
  Serial.println("[CALLBACK] saveParamCallback fired");
  Serial.println("PARAM customfieldid = " + getParam("customfieldid"));
}

void setupWifiManager()
{

  WiFi.mode( WIFI_STA ); // explicitly set mode, esp defaults to STA+AP  
  
  pinMode(TRIGGER_PIN, INPUT);
  wm.resetSettings(); // wipe settings

  if( wm_nonblocking )
    wm.setConfigPortalBlocking( false );

  // add a custom input field
  int customFieldLength = 40;


  // new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\"");
  
  // test custom html input type(checkbox)
  // new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\" type=\"checkbox\""); // custom html type
  
  // test custom html(radio)
  const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
  new (&custom_field) WiFiManagerParameter( custom_radio_str ); // custom html input
  
  wm.addParameter( &custom_field );
  wm.setSaveParamsCallback( saveParamCallback );

  // custom menu via array or vector
  // 
  // menu tokens, "wifi","wifinoscan","info","param","close","sep","erase","restart","exit" (sep is seperator) (if param is in menu, params will not show up in wifi page!)
  // const char* menu[] = {"wifi","info","param","sep","restart","exit"}; 
  // wm.setMenu(menu,6);
  std::vector<const char *> menu = {"wifi","info","param","sep","restart","exit"};
  wm.setMenu( menu );

  // set dark theme
  wm.setClass( "invert" );


  //set static ip
  // wm.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0)); // set static ip,gw,sn
  // wm.setShowStaticFields(true); // force show static ip fields
  // wm.setShowDnsFields(true);    // force show dns field always

  // wm.setConnectTimeout(20); // how long to try to connect for before continuing
  wm.setConfigPortalTimeout(30); // auto close configportal after n seconds
  // wm.setCaptivePortalEnable(false); // disable captive portal redirection
  // wm.setAPClientCheck(true); // avoid timeout if client connected to softap

  // wifi scan settings
  // wm.setRemoveDuplicateAPs(false); // do not remove duplicate ap names (true)
  // wm.setMinimumSignalQuality(20);  // set min RSSI (percentage) to show in scans, null = 8%
  // wm.setShowInfoErase(false);      // do not show erase button on info page
  // wm.setScanDispPerc(true);       // show RSSI as percentage not graph icons
  
  // wm.setBreakAfterConfig(true);   // always exit configportal even if wifi save fails

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    WiFiManager wifiManager;
    res = wifiManager.startConfigPortal( "AutoConnectAP", "password" );
  }
  else
  {
    res = wm.autoConnect( "AutoConnectAP", "password" ); // password protected ap
  }

  if( !res ) {
    Serial.println("Failed to connect or hit timeout");
    // ESP.restart();
  } 
  else {
    //if you get here you have connected to the WiFi    
    Serial.println("connected...yeey :)");
    ssid = wm.getWiFiSSID();
    password = wm.getWiFiPass();
  
  }
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
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
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
      mqttClient.subscribe( MQTT_TOPIC_RELAYSTATEUPDATE_PUB );
    } else {
      Serial.print(" fehlgeschlagen (rc=");
      Serial.print(mqttClient.state());
      Serial.println("). Neuer Versuch in 5 Sekunden...");
      delay(5000);
    }
  }
}

short timeStringToShort( String& srTime )
{
  int hours = srTime.substring(0, 2).toInt();   // "14" → 14
  int minutes = srTime.substring(3, 2).toInt(); // "30" → 30
  return (hours << 8) | minutes;  // Stunden ins High-Byte, Minuten ins Low-Byte
}

String shortToTimeString( short encodedTime )
{
  int hours = (encodedTime >> 8) & 0xFF;  // High-Byte extrahieren
  int minutes = encodedTime & 0xFF;      // Low-Byte extrahieren
  return String( (unsigned char)hours ) + ":" + String( (unsigned char) minutes );
}


// MQTT-Callback (eingehende Nachrichten)
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = payloadToString(payload, length);
  Serial.print("Nachricht auf ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);

  String sTopic( topic );
  if ( sTopic == MQTT_TOPIC_RELAY_SUB ) {
    parseJSON(message);
    updateRelays();    
  }
  else if ( sTopic == MQTT_TOPIC_RELAYSTATEUPDATE_PUB )
  {
    prepareJSON();
  }
}

// JSON-Daten verarbeiten
void parseJSON(const String &jsonString)
{
  //DynamicJsonDocument doc(4096);
  JsonDocument doc; //(4096);
  DeserializationError error = deserializeJson( doc, jsonString );

  if (error) {
    Serial.println("JSON-Parsing fehlgeschlagen!");
    return;
  }
  Serial.println( jsonString );

  JsonArray relaysArray = doc["relays"].as<JsonArray>();
  for (uint i = 0; i < relaysArray.size() && i < RELAY_COUNT; i++)
  {
    JsonObject relayObject = relaysArray[i];
    
    int idx = ((int)(relayObject["id"]));

    relays[ idx ].id = relayObject["id"];
    relays[ idx ].name = relayObject["name"].as<String>();
    relays[ idx ].state = relayObject["state"].as<String>();
    relays[ idx ].mode = relayObject["mode"].as<String>();


    JsonArray timersArray = relayObject["timers"].as<JsonArray>();
    for (uint j = 0; j < timersArray.size() && j < TIMER_PER_RELAY; j++)
    {
      JsonObject timerObject = timersArray[j];
      String timeHelper = timerObject["start"].as<String>();
      relays[ idx ].timers[ j ].sStarttime = timeStringToShort( timeHelper );
      timeHelper = timerObject["stop"].as<String>();
      relays[ idx ].timers[ j ].sStarttime = timeStringToShort( timeHelper );
      relays[ idx ].timers[ j ].active = timerObject["active"].as<bool>();
      JsonArray daysArray = timerObject["days"].as<JsonArray>();

      for(JsonVariant v : daysArray)
      {
        byte bDays = 0;
        if ( v.as<String>().lastIndexOf( "Mo" ) != -1 ) bDays |= enmDayOfWeek::monday;
        else if ( v.as<String>().lastIndexOf( "Di" ) != -1 ) bDays |= enmDayOfWeek::tuesday;
        else if ( v.as<String>().lastIndexOf( "Mi" ) != -1 ) bDays |= enmDayOfWeek::wednesday;
        else if ( v.as<String>().lastIndexOf( "Do" ) != -1 ) bDays |= enmDayOfWeek::thursday;
        else if ( v.as<String>().lastIndexOf( "Fr" ) != -1 ) bDays |= enmDayOfWeek::friday;
        else if ( v.as<String>().lastIndexOf( "Sa" ) != -1 ) bDays |= enmDayOfWeek::saturday;
        else if ( v.as<String>().lastIndexOf( "So" ) != -1 ) bDays |= enmDayOfWeek::sunday;
        else bDays = 0x80;

        relays[ idx ].timers[ j ].days = bDays;

        Serial.println(v.as<int>());
      }
    }
  }

  //updateRelays();
}

// prepare current relays state to JSON/MQTT payload
void prepareJSON( void )
{
  Serial.println( "prepareJSON" );

  JsonDocument doc;
  DeserializationError error = deserializeJson( doc, jsonTemplate );

  if (error) {
    Serial.println("JSON-Parsing fehlgeschlagen!");
    return;
  }

  //JsonArray relaysArray = doc["relays"].as<JsonArray>();
  for (uint i = 0; /* i < relaysArray.size() && */ i < RELAY_COUNT; i++)
  {
    
    doc["relays"][0]["id"] = relays[i].id;
    doc["relays"][0]["state"] = relays[i].state;
    doc["relays"][0]["name"] = relays[i].name;

    //  digitalWrite( 2, (relays[i].state == "on"));
    for ( int j = 0; j < TIMER_PER_RELAY; j++)
    {
      doc["relays"][0]["timers"][j]["active"] = relays[i].timers[j].active;
      doc["relays"][0]["timers"][j]["start"] = shortToTimeString( relays[i].timers[j].sStarttime );
      doc["relays"][0]["timers"][j]["stop"] = shortToTimeString( relays[i].timers[j].sStoptime );

      byte days = relays[i].timers[j].days;
      int dayElemJSON = 0;
      while ( days != 0 )
      {
        if ( ( relays[i].timers[j].days & enmDayOfWeek::monday ) != 0 ) { doc["relays"][0]["timers"][j]["days"][dayElemJSON++] = "Mo"; }
        if ( ( relays[i].timers[j].days & enmDayOfWeek::tuesday ) != 0 ) { doc["relays"][0]["timers"][j]["days"][dayElemJSON++] = "Di"; }
        if ( ( relays[i].timers[j].days & enmDayOfWeek::wednesday ) != 0 ) { doc["relays"][0]["timers"][j]["days"][dayElemJSON++] = "Mi"; }
        if ( ( relays[i].timers[j].days & enmDayOfWeek::thursday ) != 0 ) { doc["relays"][0]["timers"][j]["days"][dayElemJSON++] = "Do"; }
        if ( ( relays[i].timers[j].days & enmDayOfWeek::friday ) != 0 ) { doc["relays"][0]["timers"][j]["days"][dayElemJSON++] = "Fr"; }
        if ( ( relays[i].timers[j].days & enmDayOfWeek::saturday ) != 0 ) { doc["relays"][0]["timers"][j]["days"][dayElemJSON++] = "Sa"; }
        if ( ( relays[i].timers[j].days & enmDayOfWeek::sunday ) != 0 ) { doc["relays"][0]["timers"][j]["days"][dayElemJSON++] = "So"; }
      }
    }
    String jsonString;
    serializeJson(doc, jsonString);
    Serial.println( jsonString.c_str() );


    if ( !mqttClient.publish( MQTT_TOPIC_RELAY_PUB, jsonString.c_str() ) )
    {
      Serial.println( "couldn't publish the string!");
    }    
  }
}

// // Relais aktualisieren
void updateRelays()
{
  for (int i = 0; i < RELAY_COUNT; i++)
  {
    if (relays[i].mode == "manual")
    {

      // Serial.println( relays[i].id );
      // Serial.println( relays[i].name );
      // Serial.print( (relays[i].state == "on") );
      // Serial.println( relays[i].state );
      // Serial.println( relays[i].mode );
      uint8_t outVal = (relays[i].state == "on")? HIGH : LOW;
      // Serial.println( String("outVal: ") + String( outVal ) + String("relayPin: ") + String(relayPins[i]) );
      digitalWrite( relayPins[i], outVal );

    }
    else  {
      processTimers( i );
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


void processTimers( uint8_t nmbRelay )
{
    if ( relays[nmbRelay].id != -1 )
    {
      int iTargetRelayState = 0;

      for ( uint t = 0; t < TIMER_PER_RELAY && relays[ nmbRelay ].timers[t].id != -1; t++ )
      {

        if ( relays[ nmbRelay ].timers[t].active != 0 )
        {
          
          // time_t tmTimer = relays[i].timers[t].parseISO8601();
          time_t ntpTime = timeClient.getEpochTime();

          struct tm* localTime = localtime( &ntpTime );
          if (    ( ( localTime->tm_hour >= (relays[ nmbRelay ].timers[t].sStarttime >> 8 ) ) & 0xFF ) && ( ( localTime->tm_min >= (relays[ nmbRelay ].timers[t].sStarttime ) ) & 0xFF )
              &&  ( ( localTime->tm_hour <= (relays[ nmbRelay ].timers[t].sStoptime >> 8 ) ) & 0xFF ) && ( ( localTime->tm_min <= (relays[ nmbRelay ].timers[t].sStoptime ) ) & 0xFF ) ) 
          {
            iTargetRelayState = ( ( (2 << localTime->tm_wday) & relays[nmbRelay].timers[t].days ) != 0 )? 1 : 0;
          }
        }
      }

      digitalWrite( relayPins[ relays[ nmbRelay ].id ], iTargetRelayState );
 
  }
}
