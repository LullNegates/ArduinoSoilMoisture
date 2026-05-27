// ====== ESP8266 NODE CODE (Wemos D1 mini) ======
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

const int SOIL_ANALOG_PIN  = A0;
const int SOIL_DIGITAL_PIN = D5;

// ESP32 AP credentials (from hub sketch)
const char* ssid     = "SoilHub";
const char* password = "12345678";

// IP of the ESP32 AP (printed in its Serial Monitor, usually 192.168.4.1)
const char* hubHost  = "192.168.4.1";
const uint16_t hubPort = 80;

// Unique ID for this sensor, must match sensorConfigs[] on ESP32
const char* NODE_ID = "sensor1";

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("Soil node starting...");

  pinMode(SOIL_DIGITAL_PIN, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to hub WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected. Node IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  int raw     = analogRead(SOIL_ANALOG_PIN);
  int digital = digitalRead(SOIL_DIGITAL_PIN);

  // local display
  int percent = (1024 - raw) * 100 / 1024;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  Serial.print("Raw=");
  Serial.print(raw);
  Serial.print(" Percent=");
  Serial.print(percent);
  Serial.print("% Digital=");
  Serial.println(digital);

  // send to ESP32 hub
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    String url = String("http://") + hubHost + ":" + hubPort +
                 "/update?id="     + NODE_ID +
                 "&raw="           + String(raw) +
                 "&digital="       + String(digital);

    if (http.begin(client, url)) {
      int code = http.GET();
      Serial.print("Hub response: ");
      Serial.println(code);
      http.end();
    } else {
      Serial.println("HTTP begin failed");
    }
  } else {
    Serial.println("WiFi not connected");
  }

  delay(5000); // send every 5s
}
