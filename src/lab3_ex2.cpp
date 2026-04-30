#include "Arduino.h"
#include "WiFi.h"
#include <ArduinoJson.h>
#include "PubSubClient.h"

#define RED_LED 26
#define GREEN_LED 27
#define BLUE_LED 14
#define YELLOW_LED 12

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_broker = "mqtt.iotserver.uz";
const int mqtt_port = 1883;
const char* mqtt_username = "userTTPU";
const char* mqtt_password = "mqttpass";

// Switched back to kamronbek
const char* topic_red = "ttpu/iot/kamronbek/led/red";
const char* topic_green = "ttpu/iot/kamronbek/led/green";
const char* topic_blue = "ttpu/iot/kamronbek/led/blue";
const char* topic_yellow = "ttpu/iot/kamronbek/led/yellow";

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

void connectWiFi();
void connectMQTT();
void checkWiFiConnection();
void checkMQTTConnection();
void MQTT_callback(char* topic, byte* payload, unsigned int length); 

void setup() {
  Serial.begin(115200);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  delay(1000);

  // Requirement: Set all LEDs to LOW initially
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BLUE_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);

  connectWiFi();

  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(MQTT_callback);

  connectMQTT();
}

void loop() {
  checkWiFiConnection();
  checkMQTTConnection();
  mqtt_client.loop();
}
  
void MQTT_callback(char* topic, byte* payload, unsigned int length) {
  // Requirement: Print every received message
  Serial.print("\n[MQTT] Received on ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.write(payload, length);
  Serial.println();

  JsonDocument doc;
  
  // Requirement: Handle malformed JSON gracefully
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("Failed to parse JSON. Error: ");
    Serial.println(error.c_str());
    return; 
  }

  String state = doc["state"];
  bool on = (state == "ON"); 

  if (strcmp(topic, topic_red) == 0) {
    digitalWrite(RED_LED, on);
    Serial.println("[LED] Red LED -> " + state);
  }
  else if (strcmp(topic, topic_green) == 0) {
    digitalWrite(GREEN_LED, on);
    Serial.println("[LED] Green LED -> " + state);
  }
  else if (strcmp(topic, topic_blue) == 0) {
    digitalWrite(BLUE_LED, on);
    Serial.println("[LED] Blue LED -> " + state);
  }
  else if (strcmp(topic, topic_yellow) == 0) {
    digitalWrite(YELLOW_LED, on);
    Serial.println("[LED] Yellow LED -> " + state);
  }
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    connectWiFi();
  }
}

void checkMQTTConnection() {
  if (!mqtt_client.connected()) {
    Serial.println("MQTT connection lost. Reconnecting...");
    connectMQTT();
  }
}

void connectWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected! IP Address: ");
  Serial.println(WiFi.localIP()); 
}

void connectMQTT() {
  while (!mqtt_client.connected()) {
    Serial.println("Connecting to MQTT...");
    
    if (mqtt_client.connect("ESP32Client_Kamronbek", mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT broker!");
      
      // Subscribe to RED and print it
      mqtt_client.subscribe(topic_red);
      Serial.print("Subscribed to topic: ");
      Serial.println(topic_red);
      
      // Subscribe to GREEN and print it
      mqtt_client.subscribe(topic_green);
      Serial.print("Subscribed to topic: ");
      Serial.println(topic_green);
      
      // Subscribe to BLUE and print it
      mqtt_client.subscribe(topic_blue);
      Serial.print("Subscribed to topic: ");
      Serial.println(topic_blue);
      
      // Subscribe to YELLOW and print it
      mqtt_client.subscribe(topic_yellow);
      Serial.print("Subscribed to topic: ");
      Serial.println(topic_yellow);
      
    } else {
      Serial.print("Failed to connect to MQTT. State: ");
      Serial.print(mqtt_client.state());
      Serial.println(". Retrying in 5 seconds...");
      delay(5000);
    }
  }
}