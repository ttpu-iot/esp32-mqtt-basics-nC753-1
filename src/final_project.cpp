#include "Arduino.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>

// --- Current Temp Sensor Libraries (Commented out because using TMP36) ---
// #include <OneWire.h>
// #include <DallasTemperature.h>
// -----------------------------------------------------------------

// --- Fan Controller Hardware Pins ---
#define LED_DATA_PIN 26      
#define NUM_LEDS 120         
#define FAN_PWM_PIN 15       
#define FAN_TACH_PIN 32
#define PWM_CHANNEL 0 // (Kept for reference, but V3.0 handles channels automatically)
#define PWM_FREQ 25000
#define PWM_RESOLUTION 8

// --- Temperature Sensor Pins ---
// #define TEMP_PIN 4           // <-- DALLAS PIN DISABLED
#define TMP36_PIN 34            // <-- TMP36 PIN ACTIVE

// --- Network & MQTT Credentials ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_broker = "turin.iotserver.uz"; // YOUR AZURE VM IP
const int mqtt_port = 2890;
const char* mqtt_username = "userTTPU";  // Left blank for Azure
const char* mqtt_password = "mqttpass";  // Left blank for Azure

// --- MQTT 3-Folder Architecture ---
const char* topic_fan_status = "ttpu/iot/kamronbek/fan/status";
const char* topic_fan_state  = "ttpu/iot/kamronbek/fan/state"; 
const char* topic_fan_set    = "ttpu/iot/kamronbek/fan/set";   

// --- Global State Variables ---
bool acceptCommands = true;  
int currentBrightness = 100; 
String currentHexColor = "#000000"; 
int currentFanSpeed = 0;
int currentRPM = 0;
float currentTempC = 0.0;
CRGB leds[NUM_LEDS]; 

// --- Timers & Interrupts ---
volatile unsigned int rpmPulseCount = 0; 
unsigned long lastRPMCalcTime = 0;
unsigned long lastTempReadTime = 0;

// --- Global Objects (Dallas objects commented out for TMP36) ---
WiFiClient espClient;
PubSubClient mqtt_client(espClient);
// OneWire oneWire(TEMP_PIN);
// DallasTemperature tempSensor(&oneWire);
// -----------------------------------------------------------------

// --- The Hardware Trap (Counts Fan Spins) ---
void IRAM_ATTR tachometerISR() {
  rpmPulseCount++;
}

// --- THE REPORTER (Folder 2) ---
void publishStatus() {
  JsonDocument doc; 

  doc["color"] = currentHexColor;
  doc["brightness"] = currentBrightness;
  doc["speed"] = currentFanSpeed;
  doc["rpm"] = currentRPM;
  doc["temperature"] = String(currentTempC, 2) + "C";
  
  char buffer[256];
  serializeJson(doc, buffer);
  
  mqtt_client.publish(topic_fan_state, buffer);
  
  Serial.print("[MQTT] Status Report Published: ");
  Serial.println(buffer);
}

// --- THE BRAIN (Folder 3) ---
void MQTT_callback(char* topic, byte* payload, unsigned int length) {
  // Ignore empty payloads to prevent parsing errors when we wipe the broker's memory
  if (!acceptCommands || length == 0) return;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) {
    Serial.println("Failed to parse JSON.");
    return;
  }

  bool stateChanged = false;

  // 1. Check for immediate "current" status request
  if (doc["command"] == "current") {
    Serial.println("Dashboard requested current state.");
    publishStatus();
    return; 
  }

  // 2. Check for Brightness Update
  if (doc["brightness"]) {
    currentBrightness = doc["brightness"]; 
    FastLED.setBrightness(map(currentBrightness, 1, 100, 0, 255));
    stateChanged = true;
  }

  // 3. Check for Hex Color Update
  if (doc["color"]) {
    const char* hexColor = doc["color"];
    currentHexColor = String(hexColor);
    CRGB newColor = strtol(hexColor + 1, NULL, 16); 
    fill_solid(leds, NUM_LEDS, newColor);
    stateChanged = true;
  }

  // 4. Check for Motor Speed Update
  if (doc["speed"]) {
    currentFanSpeed = doc["speed"];
    int pwmValue = map(currentFanSpeed, 0, 100, 0, 255);
    
    // V3.0 FIX: Write directly to the PIN, not the channel
    ledcWrite(FAN_PWM_PIN, pwmValue);
    stateChanged = true;
  }

  // Push physical updates to the hardware and Report back
  if (stateChanged) {
    FastLED.show();
    publishStatus();
  }
}

// --- NETWORKING ---
void connectWiFi() {
  Serial.print("\nConnecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWiFi connected! IP: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  while (!mqtt_client.connected()) {
    Serial.print("Connecting to Azure MQTT broker... ");
    String client_id = "ESP32_Kamronbek_" + String(WiFi.macAddress());
    
    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connected!");
      
      // Wipe the Azure Broker's memory of any retained "sticky" commands
      mqtt_client.publish(topic_fan_set, "", true);
      
      mqtt_client.subscribe(topic_fan_set);
      mqtt_client.publish(topic_fan_status, "{\"status\": \"Connected and Ready\"}");
      publishStatus();
      
    } else {
      Serial.print("Failed. Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

// ==========================================
// SETUP 
// ==========================================
void setup() {
  Serial.begin(115200);

  // 1. Initialize LEDs
  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(map(currentBrightness, 1, 100, 0, 255));
  FastLED.clear(); 
  FastLED.show();

  // 2. Initialize Fan Motors (ESP32 Core V3.0 Update)
  // Instead of setup() and attachPin(), V3.0 does it all in one attach() command
  ledcAttach(FAN_PWM_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcWrite(FAN_PWM_PIN, 0); 

  // 3. Initialize RPM Sensor
  pinMode(FAN_TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FAN_TACH_PIN), tachometerISR, FALLING);

  // 4. Initialize Temperature Sensor
  // --- DALLAS SENSOR DISABLED ---
  // tempSensor.begin();
  // tempSensor.setWaitForConversion(false); 
  // tempSensor.requestTemperatures();       
  // ------------------------------------------------------------
  
  // (TMP36 is analog and doesn't require library setup here)

  // 5. Connect Network
  connectWiFi();
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  
  // Tell Azure to keep the connection alive for 90 seconds of silence
  mqtt_client.setKeepAlive(90);
  
  mqtt_client.setCallback(MQTT_callback);
  connectMQTT();
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) { connectWiFi(); }
  if (!mqtt_client.connected()) { connectMQTT(); }
  mqtt_client.loop();

  // --- READ RPM (Every 2 seconds) ---
  if (millis() - lastRPMCalcTime >= 2000) {
    noInterrupts();
    unsigned int pulses = rpmPulseCount;
    rpmPulseCount = 0;
    interrupts();

    currentRPM = (pulses / 2) * 30;
    lastRPMCalcTime = millis();
  }

  // --- READ TEMPERATURE (Every 5 seconds) ---
  if (millis() - lastTempReadTime >= 5000) {
    
    // --- DALLAS SENSOR DISABLED ---
    // currentTempC = tempSensor.getTempCByIndex(0);
    // tempSensor.requestTemperatures(); 
    // -----------------------------------------------------------

    // --- TMP36 SENSOR ACTIVE ---
    int adcVal = analogRead(TMP36_PIN);
    float milliVolt = adcVal * (3300.0 / 4095.0);
    currentTempC = (milliVolt - 500.0) / 10.0;
    // -------------------------------------------------------

    lastTempReadTime = millis();
  }
}
