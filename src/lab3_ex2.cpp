// - RED LED - `D26`
// - Green LED - `D27`
// - Blue LED - `D14`
// - Yellow LED - `D12`

// - Button (Active high) - `D25`
// - Light sensor (analog) - `D33`

// - LCD I2C - SDA: `D21`
// - LCD I2C - SCL: `D22`

/**************************************
 * LAB 3 - EXERCISE 2
 **************************************/


#include "Arduino.h"
#include "WiFi.h"
#include <ArduinoJson.h>
#include "PubSubClient.h"
#include <time.h>

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
const char* topic_red = "ttpu/iot/kamronbek/led/red";
const char* topic_green = "ttpu/iot/kamronbek/led/green";
const char* topic_blue = "ttpu/iot/kamronbek/led/blue";
const char* topic_yellow = "ttpu/iot/kamronbek/led/yellow";

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

void connectWiFi();
void connectMQTT();
void MQTT_callback(char* topic, byte* payload, unsigned int length); 

// Your code here - global declarations

/*************************
 * SETUP
 */
void setup()
{
  Serial.begin(115200);
  setupPinMode(RED_LED, OUTPUT);
  setupPinMode(GREEN_LED, OUTPUT);
  setupPinMode(BLUE_LED, OUTPUT);
  setupPinMode(YELLOW_LED, OUTPUT);
  // Your code here

  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(BLUE_LED, HIGH);
  digitalWrite(YELLOW_LED, HIGH);

  connectWiFi();

  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(MQTT_callback);

  connectMQTT();
}


/*************************
 * LOOP
 */
void loop() 
{
mqtt_client.loop();  // Your code here
}
