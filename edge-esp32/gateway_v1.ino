#include <WiFi.h>
#include <esp_now.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ================= WIFI CONFIG =================
const char* WIFI_SSID = "HONOR X6c";
const char* WIFI_PASSWORD = "12345678";

// ================= HIVEMQ CONFIG =================
const char* MQTT_SERVER = "baee56899d174ecca33b3c50d09bf2e7.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;

const char* MQTT_USERNAME = "clucknet3yp";
const char* MQTT_PASSWORD = "3yp_cluckNet";

const char* MQTT_TOPIC = "clucknet/zone/ZONE_01/data";

// ================= MQTT OBJECTS =================
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

// ================= DATA STRUCTURE =================
typedef struct struct_message {
  char nodeId[20];

  float avgTemperature;
  float avgHumidity;
  int avgNH3;

  int lpgRaw;
  int lpgThreshold;

  bool lpgDetected;
  bool buzzerStatus;
  bool servoClosed;

  char messageType[20];
} struct_message;

// ================= GLOBAL BUFFER =================
struct_message receivedData;
bool newDataAvailable = false;

// ================= WIFI CONNECT =================
void connectWiFi() {
  Serial.print("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.print("WiFi Channel: ");
  Serial.println(WiFi.channel());
}

// ================= MQTT CONNECT =================
void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to HiveMQ MQTT...");

    String clientId = "CluckNet-Gateway-";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("Connected.");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying...");
      delay(3000);
    }
  }
}

// ================= ESP-NOW RECEIVE =================
void onDataReceive(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  memcpy(&receivedData, incomingData, sizeof(receivedData));

  newDataAvailable = true;   // Only mark flag

  Serial.println("\nESP-NOW data received (stored)");
}

// ================= MQTT PUBLISH =================
void publishToMQTT() {
  StaticJsonDocument<512> doc;

  doc["nodeId"] = receivedData.nodeId;
  doc["messageType"] = receivedData.messageType;
  doc["avgTemperature"] = receivedData.avgTemperature;
  doc["avgHumidity"] = receivedData.avgHumidity;
  doc["avgNH3"] = receivedData.avgNH3;
  doc["lpgRaw"] = receivedData.lpgRaw;
  doc["lpgThreshold"] = receivedData.lpgThreshold;
  doc["lpgDetected"] = receivedData.lpgDetected;
  doc["buzzerStatus"] = receivedData.buzzerStatus;
  doc["servoClosed"] = receivedData.servoClosed;

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);

  bool success = mqttClient.publish(MQTT_TOPIC, jsonBuffer);

  if (success) {
    Serial.println("MQTT Publish SUCCESS:");
    Serial.println(jsonBuffer);
  } else {
    Serial.println("MQTT Publish FAILED");
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  connectWiFi();

  secureClient.setInsecure();   // For demo
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  connectMQTT();

  // ESP-NOW INIT
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }

  esp_now_register_recv_cb(onDataReceive);

  Serial.println("Gateway Ready...");
  Serial.println("Waiting for ESP-NOW data...");
}

// ================= LOOP =================
void loop() {
  if (!mqttClient.connected()) {
    connectMQTT();
  }

  mqttClient.loop();

  // Publish only when new data arrives
  if (newDataAvailable) {
    publishToMQTT();
    newDataAvailable = false;
  }
}