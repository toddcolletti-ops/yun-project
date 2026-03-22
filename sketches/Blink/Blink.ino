/*
  Blink - Arduino Yun
  Basic connectivity test sketch.
  Turns the built-in LED on for one second, then off for one second, repeatedly.
*/

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
}
