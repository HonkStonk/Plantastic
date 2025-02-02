/*
E+	red
E-	black
A-	white
A+	green
B-	not connected	not connected
B+	not connected	not connected
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include "HX711.h"

const char* ssid = "meh";
const char* password = "meh";
const char* mqtt_server = "meh";
const int mqtt_port = 1883;
const char* mqtt_user = "meh";
const char* mqtt_password = "meh";

// make def for MOISTURE_POWER = D13
// make def for MOISTURE_READ = A0
// make non-blocking 100ms warm-up-then-measure - minimize on-time to reduce corrosion
// prep for PUMP_ON = D12 - BSS138 to 5V motor supplied by USB-C (VBUS)
// with standard 40mm galvanized nails:
// 5k = water
// 12k = wet soil
// 30k = dry soil

#define HX1_CLOCK_PIN (D2)
#define HX1_DATA_PIN (D3)
#define HX1_OFFSET (-486830)
#define HX1_SCALE  (397.042847)

#define HX2_CLOCK_PIN (D4)
#define HX2_DATA_PIN (D5)
#define HX2_OFFSET (-249720)
#define HX2_SCALE  (424.912598)

#define PUMP_PIN (A7)

WiFiClient espClient;
PubSubClient client(espClient);
HX711 scalePlant;
HX711 scaleTank;

// Pump control variables
bool pumpActive = false;
unsigned long pumpStartTime = 0;
unsigned long pumpRunDuration = 0;  // in milliseconds

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(A0, INPUT); // Initialize A0 for soil moisture sensor

  scalePlant.begin(HX1_DATA_PIN, HX1_CLOCK_PIN);
  scalePlant.set_scale(HX1_SCALE);
  scalePlant.set_offset(HX1_OFFSET);
  scalePlant.set_medavg_mode();

  scaleTank.begin(HX2_DATA_PIN, HX2_CLOCK_PIN);
  scaleTank.set_scale(HX2_SCALE);
  scaleTank.set_offset(HX2_OFFSET);
  scaleTank.set_medavg_mode();
  
  pinMode(PUMP_PIN, OUTPUT);
  analogWrite(PUMP_PIN, 0);
  delay(500);
  analogWrite(PUMP_PIN, 100);
  delay(500);
  analogWrite(PUMP_PIN, 150);
  delay(500);
  analogWrite(PUMP_PIN, 50);
  delay(500);
  analogWrite(PUMP_PIN, 0);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Handle pump timing non-blocking
  if (pumpActive) {
    unsigned long now = millis();
    if (now - pumpStartTime >= pumpRunDuration) {
      // Time's up; turn the pump off
      analogWrite(PUMP_PIN, 0);
      pumpActive = false;
      Serial.println("Pump turned off");
      // Optionally, update LED status for debugging
      digitalWrite(LED_BUILTIN, LOW);
    }
  }

  // Send sensor data every 10 seconds
  static unsigned long lastMsg = 0;
  unsigned long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    
    int soilMoisture = analogRead(A0); // Read A0 value
    String message = String(soilMoisture);
    client.publish("esp32/soil_moisture", message.c_str(), true);
    Serial.print("Soil moisture: ");
    Serial.println(message);

    float plantWeight = scalePlant.get_units(15);
    message = String(plantWeight);
    client.publish("esp32/plant_weight", message.c_str(), true);
    Serial.print("Plant weight: ");
    Serial.println(message);

    float tankWeight = scaleTank.get_units(15);
    message = String(tankWeight);
    client.publish("esp32/tank_weight", message.c_str(), true);
    Serial.print("Tank weight: ");
    Serial.println(message);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.publish("esp32/status", "connected");
      client.subscribe("esp32/pump_run");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  // Convert the payload to a String
  String command = "";
  for (int i = 0; i < length; i++) {
    command += (char)payload[i];
  }
  Serial.println(command);

  if (strcmp(topic, "esp32/pump_run") == 0) {
    // Parse the payload as an integer duration in seconds
    uint8_t durationSec = command.toInt();

    // Limit the duration to 1-60 seconds
    if (durationSec < 1) {
      durationSec = 1;
    } else if (durationSec > 60) {
      durationSec = 60;
    }
    
    // Calculate run duration in milliseconds
    pumpRunDuration = durationSec * 1000UL;
    pumpStartTime = millis();
    
    // Start the pump
    analogWrite(PUMP_PIN, 100);
    pumpActive = true;
    Serial.print("Pump turned on for ");
    Serial.print(durationSec);
    Serial.println(" seconds");
    
    // Optionally, set LED for debugging (e.g., LED on while pump is running)
    digitalWrite(LED_BUILTIN, HIGH);
  }
}
