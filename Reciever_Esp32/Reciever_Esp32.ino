#include <WiFi.h>
#include <WebServer.h>

const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

WebServer server(80);

// --- Sensor storage ---
struct SensorReading {
  String id;
  int moisture;
  unsigned long lastUpdateMs;
};

const int MAX_SENSORS = 10;
SensorReading sensors[MAX_SENSORS];
int sensorCount = 0;

int findSensorIndex(const String& id) {
  for (int i = 0; i < sensorCount; i++) {
    if (sensors[i].id == id) return i;
  }
  return -1;
}

void handleUpdate() {
  if (!server.hasArg("id") || !server.hasArg("value")) {
    server.send(400, "text/plain", "Missing id or value");
    return;
  }

  String id = server.arg("id");
  int value = server.arg("value").toInt();

  int idx = findSensorIndex(id);
  if (idx == -1) {
    if (sensorCount >= MAX_SENSORS) {
      server.send(500, "text/plain", "Sensor list full");
      return;
    }
    idx = sensorCount++;
    sensors[idx].id = id;
  }

  sensors[idx].moisture = value;
  sensors[idx].lastUpdateMs = millis();

  Serial.print("Update from ");
  Serial.print(id);
  Serial.print(": ");
  Serial.println(value);

  server.send(200, "text/plain", "OK");
}

void handleData() {
  String json = "[";
  for (int i = 0; i < sensorCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"id\":\"" + sensors[i].id + "\",";
    json += "\"value\":" + String(sensors[i].moisture) + ",";
    json += "\"lastUpdateMs\":" + String(sensors[i].lastUpdateMs);
    json += "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleRoot() {
  server.send(200, "text/plain", "ESP32 hub is running. Use /data or /update.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("Starting ESP32 hub...");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/update", handleUpdate);
  server.on("/data", handleData);

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
}
