/*
  BridgeTest - Arduino Yun
  Tests the Bridge library communication between the ATmega32U4
  and the AR9331 Linux processor.
*/

#include <Bridge.h>

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(9600);
  while (!Serial);

  Serial.println("Bridge Test");
  Serial.println("Initializing Bridge...");

  Bridge.begin();

  Serial.println("Bridge initialized.");
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  // Put a value into the Bridge datastore
  Bridge.put("testKey", String(millis()));

  // Read it back
  String value = "";
  Bridge.get("testKey", value);
  Serial.print("Bridge value: ");
  Serial.println(value);

  delay(2000);
}
