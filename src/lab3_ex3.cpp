#include "Arduino.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <LiquidCrystal_I2C.h> // REQUIRED: Add "LiquidCrystal I2C" to Library Manager

// --- Pin Definitions ---
#define RED_LED 26
#define GREEN_LED 27
#define BLUE_LED 14
#define YELLOW_LED 12
#define BUTTON 25

// --- Network & MQTT Credentials ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_broker = "mqtt.iotserver.uz";  
const int mqtt_port = 1883;
const char* mqtt_username = "userTTPU";  
const char* mqtt_password = "mqttpass";  

// --- MQTT Topics ---
const char* topic_button = "ttpu/iot/kamronbek/events/button"; 
const char* topic_red    = "ttpu/iot/kamronbek/led/red";
const char* topic_green  = "ttpu/iot/kamronbek/led/green";
const char* topic_blue   = "ttpu/iot/kamronbek/led/blue";
const char* topic_yellow = "ttpu/iot/kamronbek/led/yellow";
const char* topic_display= "ttpu/iot/kamronbek/display";

// --- Global Objects & Variables ---
WiFiClient espClient;
PubSubClient mqtt_client(espClient);
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16 chars, 2 lines

int lastButtonReading = LOW;
int confirmedButtonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// --- Function Declarations ---
void connectWiFi();
void connectMQTT();
void sentButtonState(const char* State);
void updateLCD(const char* text);
void MQTT_callback(char* topic, byte* payload, unsigned int length);

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  // 1. Initialize Pins
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(BUTTON, INPUT);

  // Turn LEDs off initially
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BLUE_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);

  // 2. Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Connecting...");

  // 3. Connect WiFi
  connectWiFi();

  // 4. Synchronize NTP Time (Tashkent is UTC+5, 5 * 3600 = 18000 seconds)
  Serial.print("Syncing NTP time...");
  configTime(18000, 0, "pool.ntp.org");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nNTP Synced!");
  lcd.setCursor(0, 1);
  lcd.print("Time Synced!");

  // 5. Connect MQTT
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(MQTT_callback);
  connectMQTT();

  // Clear LCD ready for messages
  lcd.clear();
  lcd.print("Waiting for msg...");
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  // Check and maintain connections
  if (WiFi.status() != WL_CONNECTED) { connectWiFi(); }
  if (!mqtt_client.connected()) { connectMQTT(); }
  mqtt_client.loop();

  // --- Button Debounce Logic (Exercise 1) ---
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

// ==========================================
// FUNCTIONS
// ==========================================

void sentButtonState(const char* State) {
  JsonDocument doc;
  doc["event"] = State; 
  doc["timestamp"] = time(nullptr); // Current Unix timestamp

  char buffer[256];
  serializeJson(doc, buffer);

  Serial.print("[MQTT] Publishing button: ");
  Serial.println(buffer);
  mqtt_client.publish(topic_button, buffer);
}

void MQTT_callback(char* topic, byte* payload, unsigned int length) {
  // Print received message to Serial
  Serial.print("\n[MQTT] Received on ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.write(payload, length);
  Serial.println();

  // Handle malformed JSON safely
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("Failed to parse JSON. Error: ");
    Serial.println(error.c_str());
    return;
  }

  // --- Handle Display Messages ---
  if (strcmp(topic, topic_display) == 0) {
    const char* text = doc["text"];
    if (text) {
      updateLCD(text);
    }
    return; // Exit function early, no need to check LED logic
  }

  // --- Handle LED Messages (Exercise 2) ---
  const char* state = doc["state"];
  if (!state) return; // Prevent crashes if 'state' is missing

  bool on = (strcmp(state, "ON") == 0);

  if (strcmp(topic, topic_red) == 0) {
    digitalWrite(RED_LED, on);
    Serial.println(String("[LED] Red LED -> ") + state);
  } else if (strcmp(topic, topic_green) == 0) {
    digitalWrite(GREEN_LED, on);
    Serial.println(String("[LED] Green LED -> ") + state);
  } else if (strcmp(topic, topic_blue) == 0) {
    digitalWrite(BLUE_LED, on);
    Serial.println(String("[LED] Blue LED -> ") + state);
  } else if (strcmp(topic, topic_yellow) == 0) {
    digitalWrite(YELLOW_LED, on);
    Serial.println(String("[LED] Yellow LED -> ") + state);
  }
}

void updateLCD(const char* text) {
  // 1. Get current time from the ESP32
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  
  // 2. Format the timestamp: DD/MM HH:MM:SS
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%d/%m %H:%M:%S", &timeinfo);
  
  // 3. Truncate text if it's over 16 characters
  String message = String(text);
  if (message.length() > 16) {
    message = message.substring(0, 16);
  }

  // 4. Print to LCD
  lcd.clear();
  lcd.setCursor(0, 0); // Line 1
  lcd.print(message);
  lcd.setCursor(0, 1); // Line 2
  lcd.print(timeString);

  // 5. Serial confirmation
  Serial.println("[LCD] Screen updated successfully!");
}

void connectWiFi() {
  Serial.print("\nConnecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWiFi connected! IP Address: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  while (!mqtt_client.connected()) {
    Serial.print("Connecting to MQTT broker... ");
    
    // Create unique client ID
    String client_id = "ESP32_Kamronbek_" + String(WiFi.macAddress());
    
    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connected!");
      
      // Subscribe to all topics
      mqtt_client.subscribe(topic_red);
      mqtt_client.subscribe(topic_green);
      mqtt_client.subscribe(topic_blue);
      mqtt_client.subscribe(topic_yellow);
      mqtt_client.subscribe(topic_display);
      
      Serial.println("Subscribed to 4 LED topics and 1 Display topic.");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(". Retrying in 5 seconds...");
      delay(5000);
    }
  }
}