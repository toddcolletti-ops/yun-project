/*
  AnalogSensor - Arduino Yun
  Reads any analog sensor (LDR, potentiometer, thermistor, soil moisture, etc.)
  and publishes values to the Bridge datastore + REST API.

  Wiring:
    Sensor output -> A0
    (Most analog sensors: VCC->5V, GND->GND, OUT->A0)

  Access readings via: http://192.168.11.36/arduino/analog
*/

#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>

BridgeServer server;

const int SENSOR_PIN = A0;
const unsigned long READ_INTERVAL = 1000;

int rawValue = 0;
float voltage = 0.0;
unsigned long lastRead = 0;
int readCount = 0;
long runningTotal = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(9600);

  Bridge.begin();
  server.listenOnLocalhost();
  server.begin();

  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("AnalogSensor ready");
}

void loop() {
  unsigned long now = millis();

  if (now - lastRead >= READ_INTERVAL) {
    lastRead = now;
    rawValue = analogRead(SENSOR_PIN);
    voltage = rawValue * (5.0 / 1023.0);
    readCount++;
    runningTotal += rawValue;

    Bridge.put("analog_raw", String(rawValue));
    Bridge.put("analog_voltage", String(voltage, 3));
    Bridge.put("analog_avg", String(runningTotal / readCount));
    Bridge.put("analog_reads", String(readCount));

    Serial.print("Raw: ");
    Serial.print(rawValue);
    Serial.print("  Voltage: ");
    Serial.print(voltage, 3);
    Serial.println("V");
  }

  BridgeClient client = server.accept();
  if (client) {
    String command = client.readStringUntil('/');
    command.trim();

    if (command == "analog") {
      client.print("{\"raw\":");
      client.print(rawValue);
      client.print(",\"voltage\":");
      client.print(voltage, 3);
      client.print(",\"average\":");
      client.print(runningTotal / max(readCount, 1));
      client.print(",\"reads\":");
      client.print(readCount);
      client.print("}");
    } else {
      client.print("{\"error\":\"unknown command\"}");
    }
    client.stop();
  }
}
