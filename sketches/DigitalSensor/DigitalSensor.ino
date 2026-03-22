/*
  DigitalSensor - Arduino Yun
  Reads any digital sensor (PIR, tilt switch, reed switch, button, IR break-beam, etc.)
  with debouncing and event counting. Publishes to Bridge + REST API.

  Wiring:
    Sensor output -> D2 (interrupt-capable pin)
    (Most digital sensors: VCC->5V, GND->GND, OUT->D2)

  Access readings via: http://192.168.11.36/arduino/digital
*/

#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>

BridgeServer server;

const int SENSOR_PIN = 2;
const unsigned long DEBOUNCE_MS = 50;
const unsigned long UPDATE_INTERVAL = 500;

volatile bool stateChanged = false;
int currentState = LOW;
int lastState = LOW;
unsigned long lastDebounce = 0;
unsigned long lastUpdate = 0;
unsigned long triggerCount = 0;
unsigned long lastTriggerTime = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);

  Serial.begin(9600);

  Bridge.begin();
  server.listenOnLocalhost();
  server.begin();

  currentState = digitalRead(SENSOR_PIN);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("DigitalSensor ready");
}

void loop() {
  unsigned long now = millis();

  // Debounced read
  int reading = digitalRead(SENSOR_PIN);
  if (reading != lastState) {
    lastDebounce = now;
  }
  if ((now - lastDebounce) > DEBOUNCE_MS) {
    if (reading != currentState) {
      currentState = reading;
      if (currentState == HIGH) {
        triggerCount++;
        lastTriggerTime = now;
        Serial.print("TRIGGERED #");
        Serial.println(triggerCount);
      }
    }
  }
  lastState = reading;

  // Periodic Bridge update
  if (now - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = now;
    Bridge.put("digital_state", currentState == HIGH ? "1" : "0");
    Bridge.put("digital_triggers", String(triggerCount));
    Bridge.put("digital_last_ms", String(lastTriggerTime));
  }

  // REST API
  BridgeClient client = server.accept();
  if (client) {
    String command = client.readStringUntil('/');
    command.trim();

    if (command == "digital") {
      client.print("{\"state\":");
      client.print(currentState);
      client.print(",\"triggers\":");
      client.print(triggerCount);
      client.print(",\"last_trigger_ms\":");
      client.print(lastTriggerTime);
      client.print(",\"uptime_ms\":");
      client.print(now);
      client.print("}");
    } else if (command == "reset") {
      triggerCount = 0;
      lastTriggerTime = 0;
      client.print("{\"status\":\"counters reset\"}");
    } else {
      client.print("{\"error\":\"unknown command\"}");
    }
    client.stop();
  }
}
