void setup() {
  Serial.begin(115200);
  bmsSetup();
  displaySetup();
}

void loop() {
  bmsLoop();
  displayLoop();
}
