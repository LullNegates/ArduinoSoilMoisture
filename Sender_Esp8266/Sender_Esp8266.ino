const int SOIL_PIN = A0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Soil Sensor Test...");
}

void loop() {
  int raw = analogRead(SOIL_PIN);

  // Convert roughly to percentage (you can tune later)
  int moisture = map(raw, 1023, 300, 0, 100);
  moisture = constrain(moisture, 0, 100);

  Serial.print("Raw = ");
  Serial.print(raw);
  Serial.print("    Moisture = ");
  Serial.print(moisture);
  Serial.println("%");

  delay(1000);
}
