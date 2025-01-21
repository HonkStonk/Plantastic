#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "meh";
const char* password = "meh";
const char* mqtt_server = "meh";
const int mqtt_port = 1883;
const char* mqtt_user = "meh";
const char* mqtt_password = "meh";

// meke def for MOISTURE_POWER = D13
// make def for MOISTURE_READ = A0
// make non-blocking 100ms warm-up-then-measure - minimize on-time to reduce corrosion
// prep for PUMP_ON = D12 - BSS138 to 5V motor supplied by USB-C (VBUS)
// with standard 40mm galvanized nails:
// 5k = water
// 12k = wet soil
// 30k = dry soil

WiFiClient espClient;
PubSubClient client(espClient);

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
  pinMode(A0, INPUT); // Initialize A0 for soil moisture sensor
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Send soil moisture data every 10 seconds
  static unsigned long lastMsg = 0;
  unsigned long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    
    int soilMoisture = analogRead(A0); // Read A0 value
    String message = String(soilMoisture);
    
    client.publish("esp32/soil_moisture", message.c_str(), true);
    Serial.print("Soil Moisture: ");
    Serial.println(message);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.publish("esp32/status", "connected");
      client.subscribe("esp32/command");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, int length) {
  static bool state;

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strcmp(topic, "esp32/command") == 0) {
    String command = "";
    for (int i = 0; i < length; i++) {
      command += (char)payload[i];
    }

    Serial.print("Received command: ");
    Serial.println(command);

    if (command == "ON") {
      if (state) {
        digitalWrite(LED_BUILTIN, HIGH);
        Serial.println("LED turned on");
        state = 0;
      } else {
        digitalWrite(LED_BUILTIN, LOW);
        Serial.println("LED turned off");
        state = 1;
      }
    } else if (command == "OFF") {
      digitalWrite(LED_BUILTIN, LOW);
      Serial.println("LED turned off");
    }
  }
}