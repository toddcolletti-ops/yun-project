/*
  OneWireSensor - Arduino Yun
  Reads Dallas/Maxim 1-Wire temperature sensors (DS18B20, DS18S20, DS1822).
  Auto-discovers all sensors on the bus. Publishes to Bridge + REST API.

  Wiring:
    Data  -> D4 (with 4.7k pull-up resistor to VCC)
    VCC   -> 5V (or parasitic power: tie VCC to GND)
    GND   -> GND

  Access readings via: http://192.168.11.36/arduino/temp

  Requires OneWire and DallasTemperature libraries.
  Install via: arduino-cli lib install "OneWire" "DallasTemperature"
*/

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>

BridgeServer server;

const int ONE_WIRE_PIN = 4;
const unsigned long READ_INTERVAL = 2000;
const int MAX_SENSORS = 8;

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

int sensorCount = 0;
float temperatures[MAX_SENSORS];
DeviceAddress sensorAddresses[MAX_SENSORS];
unsigned long lastRead = 0;

void addressToString(DeviceAddress addr, char *buf) {
  for (int i = 0; i < 8; i++) {
    sprintf(buf + (i * 2), "%02X", addr[i]);
  }
  buf[16] = '\0';
}

void discoverSensors() {
  sensors.begin();
  sensorCount = min((int)sensors.getDeviceCount(), MAX_SENSORS);

  Serial.print("Found ");
  Serial.print(sensorCount);
  Serial.println(" sensor(s)");

  for (int i = 0; i < sensorCount; i++) {
    if (sensors.getAddress(sensorAddresses[i], i)) {
      sensors.setResolution(sensorAddresses[i], 12);  // 12-bit resolution
      char addrStr[17];
      addressToString(sensorAddresses[i], addrStr);
      Serial.print("  Sensor ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(addrStr);
    }
  }

  Bridge.put("ow_count", String(sensorCount));
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(9600);

  Bridge.begin();
  server.listenOnLocalhost();
  server.begin();

  discoverSensors();

  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("OneWireSensor ready");
}

void loop() {
  unsigned long now = millis();

  if (now - lastRead >= READ_INTERVAL) {
    lastRead = now;

    sensors.requestTemperatures();

    for (int i = 0; i < sensorCount; i++) {
      temperatures[i] = sensors.getTempC(sensorAddresses[i]);

      String key = "ow_temp_" + String(i);
      Bridge.put(key.c_str(), String(temperatures[i], 2));

      Serial.print("Sensor ");
      Serial.print(i);
      Serial.print(": ");
      Serial.print(temperatures[i], 2);
      Serial.println(" C");
    }
  }

  BridgeClient client = server.accept();
  if (client) {
    String command = client.readStringUntil('/');
    command.trim();

    if (command == "temp") {
      client.print("{\"sensors\":");
      client.print(sensorCount);
      client.print(",\"readings\":[");
      for (int i = 0; i < sensorCount; i++) {
        if (i > 0) client.print(",");
        char addrStr[17];
        addressToString(sensorAddresses[i], addrStr);
        client.print("{\"id\":\"");
        client.print(addrStr);
        client.print("\",\"temp_c\":");
        client.print(temperatures[i], 2);
        client.print(",\"temp_f\":");
        client.print(temperatures[i] * 9.0 / 5.0 + 32.0, 2);
        client.print("}");
      }
      client.print("]}");
    } else if (command == "scan") {
      discoverSensors();
      client.print("{\"status\":\"rescan complete\",\"sensors\":");
      client.print(sensorCount);
      client.print("}");
    } else {
      client.print("{\"error\":\"unknown command\"}");
    }
    client.stop();
  }
}
