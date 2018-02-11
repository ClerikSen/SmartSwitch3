/*
   Simpleton Sonoff Touch firmware with MQTT support
   Supports OTA update
   David Pye (C) 2016 GNU GPL v3
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <OneWire.h>
#include <ArduinoOTA.h>

#define NAME "lswitch1"
#define SSID "DIR-300NRU"
#define PASS "7581730dd"

//Defaults to DHCP, if you want a static IP, uncomment and
//configure below
//#define STATIC_IP
#ifdef STATIC_IP
IPAddress ip(192, 168, 0, 50);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
#endif

#define MQTT_SERVER "m13.cloudmqtt.com"
#define MQTT_PORT 18076
#define MQTT_USER "xlgdggka"
#define MQTT_PASS "grUisx_T6fro"
#define BUFFER_SIZE 100

#define OTA_PASS "UPDATE_PW"
#define OTA_PORT 8266
const char *cmndTopicMode = "cmnd/" NAME "/mode";
const char *cmndTopic1 = "cmnd/" NAME "/light1";
const char *cmndTopic2 = "cmnd/" NAME "/light2";
const char *cmndTopic3 = "cmnd/" NAME "/light3";
const char *statusTopic_1 = "status/" NAME "/light1";
const char *statusTopic_2 = "status/" NAME "/light2";
const char *statusTopic_3 = "status/" NAME "/light3";

#define BUTTON_PIN_1 0
#define BUTTON_PIN_2 9
#define BUTTON_PIN_3 10
#define RELAY_PIN_1 12
#define RELAY_PIN_2 5
#define RELAY_PIN_3 4
#define LED_PIN 13

volatile int desiredRelayState_1 = 0;
volatile int desiredRelayState_2 = 0;
volatile int desiredRelayState_3 = 0;
volatile int relayState_1 = 0;
volatile int relayState_2 = 0;
volatile int relayState_3 = 0;
volatile unsigned long millisSinceChange_1 = 0;
volatile unsigned long millisSinceChange_2 = 0;
volatile unsigned long millisSinceChange_3 = 0;
volatile int modes = 0;  //0-ручной, 1-автоматический
volatile unsigned long millisAttach = 0;

unsigned long lastMQTTCheck = -5000; //This will force an immediate check on init.

WiFiClient espClient;
PubSubClient client(espClient);
bool printedWifiToSerial = false;

void initWifi() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SSID);
#ifdef STATIC_IP
  WiFi.config(ip, gateway, subnet);
#endif

  WiFi.begin(SSID, PASS);
}

void checkMQTTConnection() {
  Serial.print("Checking MQTT connection: ");
  if (client.connected()) Serial.println("OK");
  else {
    if (WiFi.status() == WL_CONNECTED) {
      //Wifi connected, attempt to connect to server
      Serial.print("new connection: ");
      if (client.connect(NAME,MQTT_USER,MQTT_PASS)) {
        Serial.println("connected");
        client.subscribe(cmndTopic1);
        client.subscribe(cmndTopic2);
        client.subscribe(cmndTopic3);
        client.subscribe(cmndTopicMode);
      } else {
        Serial.print("failed, rc=");
        Serial.println(client.state());
      }
    }
    else {
      //Wifi isn't connected, so no point in trying now.
      Serial.println(" Not connected to WiFI AP, abandoned connect.");
    }
  }
  //Set the status LED to ON if we are connected to the MQTT server
  if (client.connected()) digitalWrite(LED_PIN, LOW);
    else digitalWrite(LED_PIN, HIGH);
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");

  if (!strcmp(topic, cmndTopic1)) {
    if ((char)payload[0] == '1' || ! strncasecmp_P((char *)payload, "on", length)) {
        desiredRelayState_1 = 1;
        millisAttach = millis();
    }
    else if ((char)payload[0] == '0' || ! strncasecmp_P((char *)payload, "off", length)) {
      desiredRelayState_1 = 0;
      millisAttach = millis();
    }
    else if ( ! strncasecmp_P((char *)payload, "toggle", length)) {
      desiredRelayState_1 = !desiredRelayState_1;
      millisAttach = millis();
    }
  }else if(!strcmp(topic, cmndTopic2)){
    if ((char)payload[0] == '1' || ! strncasecmp_P((char *)payload, "on", length)) {
        desiredRelayState_2 = 1;
        millisAttach = millis();
    }
    else if ((char)payload[0] == '0' || ! strncasecmp_P((char *)payload, "off", length)) {
      desiredRelayState_2 = 0;
      millisAttach = millis();
    }
    else if ( ! strncasecmp_P((char *)payload, "toggle", length)) {
      desiredRelayState_2 = !desiredRelayState_2;
      millisAttach = millis();
    }
  }else if(!strcmp(topic, cmndTopic3)){
    if ((char)payload[0] == '1' || ! strncasecmp_P((char *)payload, "on", length)) {
        desiredRelayState_3 = 1;
        millisAttach = millis();
    }
    else if ((char)payload[0] == '0' || ! strncasecmp_P((char *)payload, "off", length)) {
      desiredRelayState_3 = 0;
      millisAttach = millis();
    }
    else if ( ! strncasecmp_P((char *)payload, "toggle", length)) {
      desiredRelayState_3 = !desiredRelayState_3;
      millisAttach = millis();
    }
  }
}

void shortPress_1() {
  desiredRelayState_1 = !desiredRelayState_1; //Toggle relay state.
  millisAttach = millis();
}

void shortPress_2() {
  desiredRelayState_2 = !desiredRelayState_2; //Toggle relay state.
  millisAttach = millis();
}

void shortPress_3() {
  desiredRelayState_3 = !desiredRelayState_3; //Toggle relay state.
  millisAttach = millis();
}


void buttonChangeCallback_1() {
  if (digitalRead(BUTTON_PIN_1) == 1) {
    //Button has been released, trigger one of the two possible options.
    if (millis() - millisSinceChange_1 > 100){
      //Short press
      shortPress_1();
    }else {
      //Too short to register as a press
    }
  }else {
    //Just been pressed - do nothing until released. 
    millisSinceChange_1 = millis();
  }
 
}

void buttonChangeCallback_2() {
  if (digitalRead(BUTTON_PIN_2) == 1) {
    //Button has been released, trigger one of the two possible options.
    if (millis() - millisSinceChange_2 > 100){
      //Short press
      shortPress_2();
    }else {
      //Too short to register as a press
    }
  }else {
    //Just been pressed - do nothing until released.
    millisSinceChange_2 = millis();
  }
  
}

void buttonChangeCallback_3() {
  if (digitalRead(BUTTON_PIN_3) == 1) {
    //Button has been released, trigger one of the two possible options.
    if (millis() - millisSinceChange_3 > 100){
      //Short press
      shortPress_3();
    }else {
      //Too short to register as a press
    }
  }else {
    //Just been pressed - do nothing until released.
    millisSinceChange_3 = millis();
  }
  
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initialising");
  pinMode(RELAY_PIN_1, OUTPUT);
  pinMode(RELAY_PIN_2, OUTPUT);
  pinMode(RELAY_PIN_3, OUTPUT);
  digitalWrite(RELAY_PIN_1, LOW);
  digitalWrite(RELAY_PIN_2, LOW);
  digitalWrite(RELAY_PIN_3, LOW);
  pinMode(BUTTON_PIN_1, INPUT_PULLUP);
  pinMode(BUTTON_PIN_2, INPUT_PULLUP);
  pinMode(BUTTON_PIN_3, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, HIGH); //LED off.

  initWifi();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(MQTTcallback);

  //OTA setup
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname(NAME);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.begin();
  //Enable interrupt for button press

  Serial.println("Enabling touch switch interrupt");
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_1), buttonChangeCallback_1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_2), buttonChangeCallback_2, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_3), buttonChangeCallback_3, CHANGE);
}

void loop() {
  //If we haven't printed WiFi details to Serial port yet, and WiFi now connected,
  //do so now. (just the once)
  //Serial.println("WiFi connected");
  //delay(500);
  if (!printedWifiToSerial && WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    printedWifiToSerial = true;
  }

  if (millis() - lastMQTTCheck >= 5000) {
    checkMQTTConnection();
    lastMQTTCheck = millis();
  }

  //Handle any pending MQTT messages
  client.loop();

  //Handle any pending OTA SW updates
  ArduinoOTA.handle();

  //Relay state is updated via the interrupt *OR* the MQTT callback.
  if (relayState_1 != desiredRelayState_1) {
      Serial.print("Changing state to ");
      Serial.println(desiredRelayState_1);

      digitalWrite(RELAY_PIN_1, desiredRelayState_1);
      relayState_1 = desiredRelayState_1;

      Serial.print("Sending MQTT status update ");
      Serial.print(relayState_1);
      Serial.print(" to ");
      Serial.println(statusTopic_1);

      client.publish(statusTopic_1, relayState_1 == 0 ? "0" : "1");
  }

  if (relayState_2 != desiredRelayState_2) {
      Serial.print("Changing state to ");
      Serial.println(desiredRelayState_2);

      digitalWrite(RELAY_PIN_2, desiredRelayState_2);
      relayState_2 = desiredRelayState_2;

      Serial.print("Sending MQTT status update ");
      Serial.print(relayState_2);
      Serial.print(" to ");
      Serial.println(statusTopic_2);

      client.publish(statusTopic_2, relayState_2 == 0 ? "0" : "1");
  }

  if (relayState_3 != desiredRelayState_3) {
      Serial.print("Changing state to ");
      Serial.println(desiredRelayState_3);

      digitalWrite(RELAY_PIN_3, desiredRelayState_3);
      relayState_3 = desiredRelayState_3;

      Serial.print("Sending MQTT status update ");
      Serial.print(relayState_3);
      Serial.print(" to ");
      Serial.println(statusTopic_3);

      client.publish(statusTopic_3, relayState_3 == 0 ? "0" : "1");
  }
  
  delay(50);
}

