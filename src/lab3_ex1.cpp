#include "Arduino.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>


const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_broker = "mqtt.iotserver.uz";  
const int mqtt_port = 1883;
const char* mqtt_username = "userTTPU";  
const char* mqtt_password = "mqttpass";  

// global declarations
#define RED_LED 26
#define GREEN_LED 27
#define BLUE_LED 14
#define YELLOW_LED 12
#define BUTTON 25
#define LIGHT_SENSOR 33

const char* topic_sensor = "ttpu/iot/kamronbek/sensors/light";
const char* topic_button = "ttpu/iot/kamronbek/events/button"; 

unsigned long lastPublishTime = 0;
const long publishInterval = 5000;  
int lastButtonReading = LOW;
int confirmedButtonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// functions
void publishEveryFive();
void connectMQTT();
void connectWiFi();
void check_connections();
void SetUpPinMode();
void sentButtonState(const char* State);

void setup(){
  // 115200 is much faster and standard for the ESP32
  Serial.begin(115200);
  SetUpPinMode();

  configTime(18000, 0, "pool.ntp.org"); // UTC+5 for Tashkent

  connectWiFi();
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  connectMQTT();
}

void loop() {
  check_connections(); 
  mqtt_client.loop();
  publishEveryFive();

  int reading = digitalRead(BUTTON);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != confirmedButtonState) {
      confirmedButtonState = reading;
      
      if (confirmedButtonState == HIGH) {
        sentButtonState("PRESSED");
      } else {
        sentButtonState("RELEASED");
      }
    }
  }
  lastButtonReading = reading; 
}

void sentButtonState(const char* State){
    JsonDocument state;
    state["event"] = State; 
    state["timestamp"] = time(nullptr);

    char buffer[256];
    serializeJson(state, buffer);

    Serial.print("[MQTT] Publishing button: ");
    Serial.println(buffer);
    mqtt_client.publish(topic_button, buffer);
}

void publishEveryFive(){
  unsigned long currentTime = millis();
  if (currentTime - lastPublishTime >= publishInterval) {
    lastPublishTime = currentTime;

    JsonDocument doc;
    doc["light"] = analogRead(LIGHT_SENSOR);
    doc["timestamp"] = time(nullptr);

    char buffer[256];
    serializeJson(doc, buffer);
    
    Serial.print("[MQTT] Publishing sensor: ");
    Serial.println(buffer);
    mqtt_client.publish(topic_sensor, buffer);
  }
}

void check_connections(){
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");
    connectWiFi();
  } else if (!mqtt_client.connected()) {
    Serial.println("MQTT disconnected! Reconnecting...");
    connectMQTT();
  }
}

void connectWiFi() {
  Serial.println("\nConnecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
}

void connectMQTT() {
  while (!mqtt_client.connected()) {
    Serial.print("Connecting to MQTT... ");
    String client_id = "esp32kamronbek" + String(WiFi.macAddress());
    
    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connected!");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void SetUpPinMode(){
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(BUTTON, INPUT);
}