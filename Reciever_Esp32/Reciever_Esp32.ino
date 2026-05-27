#include <WiFi.h>
#include <WebServer.h>

struct PlantProfile;
struct SensorConfig;
struct SensorState;

// ===== ACCESS POINT SETTINGS =====
const char* apSsid     = "SoilHub";
const char* apPassword = "12345678";

WebServer server(80);

// Timestamp helper
String ts() {
  return "[" + String(millis() / 1000) + "s] ";
}

// ===== PLANT PROFILES =====
struct PlantProfile {
  const char* code;
  const char* label;
  int dryThresholdPercent;
};

PlantProfile plantProfiles[] = {
  { "tropical",  "Tropical Plant", 60 },
  { "succulent", "Succulent",      30 },
  { "herb",      "Herb",           45 }
};

const int PLANT_PROFILE_COUNT =
  sizeof(plantProfiles) / sizeof(plantProfiles[0]);

const PlantProfile* findPlantProfile(const String& code) {
  for (int i = 0; i < PLANT_PROFILE_COUNT; i++) {
    if (code.equalsIgnoreCase(plantProfiles[i].code)) {
      return &plantProfiles[i];
    }
  }
  Serial.println(ts() + "WARN: Unknown plant profile: " + code);
  return nullptr;
}

// ===== SENSOR CONFIGS =====
struct SensorConfig {
  const char* id;
  const char* displayName;
  const char* profileCode;
};

SensorConfig sensorConfigs[] = {
  { "sensor1", "Banana Plant",   "tropical" },
  { "sensor2", "Aloe Vera",      "succulent" },
  { "sensor3", "Basil Window",   "herb" }
};

const int SENSOR_CONFIG_COUNT =
  sizeof(sensorConfigs) / sizeof(sensorConfigs[0]);

// ===== RUNTIME SENSOR STATES =====
struct SensorState {
  bool used = false;
  String id;
  String name;
  const PlantProfile* profile = nullptr;
  int raw = 0;
  int percent = 0;
  int digital = 0;
  String status;
  unsigned long lastUpdateMs = 0;
};

const int MAX_SENSORS = 10;
SensorState sensors[MAX_SENSORS];

// Finds or creates a slot for sensor 'id'
int findStateIndex(const String& id) {
  // Look for existing
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (sensors[i].used && sensors[i].id == id) {
      return i;
    }
  }

  // Create new entry
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!sensors[i].used) {
      sensors[i].used = true;
      sensors[i].id = id;

      // Apply config
      const SensorConfig* cfg = nullptr;
      for (int j = 0; j < SENSOR_CONFIG_COUNT; j++) {
        if (id == sensorConfigs[j].id) {
          cfg = &sensorConfigs[j];
          break;
        }
      }

      if (cfg) {
        sensors[i].name = cfg->displayName;
        sensors[i].profile = findPlantProfile(cfg->profileCode);

        Serial.println(ts() + "INFO: Registered new sensor: " + sensors[i].name +
                       " (" + id + "), profile=" + cfg->profileCode);
      } else {
        sensors[i].name = "Unknown (" + id + ")";
        sensors[i].profile = nullptr;
        Serial.println(ts() + "INFO: Registered sensor with no config: " + id);
      }

      return i;
    }
  }

  Serial.println(ts() + "ERROR: No free sensor slots left!");
  return -1;
}

// Convert raw to %
int computePercentFromRaw(int raw) {
  int percent = (1024 - raw) * 100 / 1024;
  percent = max(0, min(100, percent));
  return percent;
}

// Determine wet/dry
String classifyStatus(int percent, int digital, const PlantProfile* profile) {
  int threshold = profile ? profile->dryThresholdPercent : 40;

  bool wetAnalog  = percent >= threshold;
  bool wetDigital = (digital == 0);

  if (wetAnalog || wetDigital) return "WET";
  return "DRY";
}

// ===== HTTP HANDLERS =====

// Log every time frontend loads "/"
void handleRootHtml() {
  Serial.println(ts() + "FRONTEND: Loaded / (main UI)");
  String html = R"(
<!DOCTYPE html>
<html>
<head>
<title>SoilHub</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family:Arial; background:#111; color:#eee; padding:1rem; }
table { border-collapse:collapse; width:100%; }
th,td { border:1px solid #444; padding:0.5rem; }
th { background:#222; }
.status-wet { color:#4caf50; font-weight:bold; }
.status-dry { color:#ff5252; font-weight:bold; }
</style>
<script>
async function refresh(){
  const res = await fetch('/data');
  const data = await res.json();
  const tbody = document.getElementById('tbody');
  tbody.innerHTML = '';
  data.forEach(row => {
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td>${row.name}</td>
      <td>${row.plantType}</td>
      <td>${row.percent}%</td>
      <td>${row.digital}</td>
      <td class="status-${row.status.toLowerCase()}">${row.status}</td>
      <td>${row.ageSec}s ago</td>
    `;
    tbody.appendChild(tr);
  });
}
setInterval(refresh, 3000);
window.onload = refresh;
</script>
</head>
<body>
<h1>Soil Moisture Hub</h1>
<table>
 <thead><tr>
   <th>Name</th><th>Plant type</th><th>Moisture %</th>
   <th>D0</th><th>Status</th><th>Last update</th>
 </tr></thead>
 <tbody id="tbody"><tr><td colspan="6">Loading...</td></tr></tbody>
</table>
</body>
</html>
  )";
  server.send(200, "text/html", html);
}

// Log whenever frontend requests /data
void handleDataJson() {
  Serial.println(ts() + "FRONTEND: Requested /data (table refresh)");

  String json = "[";
  bool first = true;
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!sensors[i].used) continue;
    if (!first) json += ",";
    first = false;

    unsigned long ageSec = (millis() - sensors[i].lastUpdateMs) / 1000;

    json += "{";
    json += "\"id\":\"" + sensors[i].id + "\",";
    json += "\"name\":\"" + sensors[i].name + "\",";
    json += "\"plantType\":\"" + 
            String(sensors[i].profile ? sensors[i].profile->label : "Default") +
            "\",";
    json += "\"percent\":" + String(sensors[i].percent) + ",";
    json += "\"digital\":" + String(sensors[i].digital) + ",";
    json += "\"status\":\"" + sensors[i].status + "\",";
    json += "\"ageSec\":" + String(ageSec);
    json += "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

// Log whenever the ESP8266 node sends data
void handleUpdate() {
  Serial.println(ts() + "UPDATE: Incoming update request");

  if (!server.hasArg("id") || !server.hasArg("raw") || !server.hasArg("digital")) {
    Serial.println(ts() + "ERROR: Missing parameters");
    server.send(400, "text/plain", "Missing id/raw/digital");
    return;
  }

  String id      = server.arg("id");
  int raw        = server.arg("raw").toInt();
  int digital    = server.arg("digital").toInt();
  int percent    = computePercentFromRaw(raw);

  Serial.println(ts() + "RAW incoming => id=" + id +
                 ", raw=" + raw + ", digital=" + digital +
                 ", percent=" + percent);

  int idx = findStateIndex(id);
  if (idx < 0) {
    server.send(500, "text/plain", "Sensor list full");
    return;
  }

  SensorState& s = sensors[idx];
  s.raw      = raw;
  s.percent  = percent;
  s.digital  = digital;
  s.status   = classifyStatus(percent, digital, s.profile);
  s.lastUpdateMs = millis();

  Serial.println(ts() + "UPDATED: " + s.name +
                 " => " + s.status +
                 " (" + percent + "%)");

  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("=========================================");
  Serial.println("        SoilHub ESP32 - STARTING         ");
  Serial.println("=========================================");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid, apPassword);

  Serial.println(ts() + "WiFi AP started.");
  Serial.println(ts() + String("SSID: ") + apSsid);
  Serial.println(ts() + "Password: " + apPassword);
  Serial.println(ts() + "AP IP: " + WiFi.softAPIP().toString());

  Serial.println(ts() + "HTTP server starting...");

  server.on("/", handleRootHtml);
  server.on("/data", handleDataJson);
  server.on("/update", handleUpdate);

  server.begin();
  Serial.println(ts() + "HTTP server ready.");
}

void loop() {
  server.handleClient();
}
