/*
  MultiSensor - Arduino Yun
  Reads multiple sensor types simultaneously and publishes all data
  to the Bridge datastore and REST API. Use this as a starting point
  when testing several sensors at once.

  Default pin assignments:
    A0 - Analog sensor (LDR, thermistor, potentiometer, etc.)
    A1 - Second analog sensor
    D2 - Digital sensor (PIR, button, switch)
    D4 - OneWire bus (DS18B20 with 4.7k pull-up)
    SDA/SCL - I2C bus

  Access readings via:
    http://192.168.11.36/arduino/all     - all sensor data
    http://192.168.11.36/arduino/analog   - analog channels only
    http://192.168.11.36/arduino/digital  - digital input only
    http://192.168.11.36/arduino/log      - recent readings as CSV

  Requires: OneWire, DallasTemperature libraries (only if using 1-Wire)
  Install via: arduino-cli lib install "OneWire" "DallasTemperature"
*/

#include <Wire.h>
#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>

BridgeServer server;

// Pin assignments
const int ANALOG_0 = A0;
const int ANALOG_1 = A1;
const int DIGITAL_IN = 2;

// Timing
const unsigned long SENSOR_INTERVAL = 1000;
const unsigned long LOG_INTERVAL = 5000;
unsigned long lastSensorRead = 0;
unsigned long lastLog = 0;

// Sensor state
int analog0 = 0;
int analog1 = 0;
int digitalState = 0;
unsigned long digitalTriggers = 0;
int lastDigital = LOW;
unsigned long lastDebounce = 0;

// I2C scan results
uint8_t i2cDevices[8];
int i2cCount = 0;

// Simple ring buffer for logging (last 20 readings)
const int LOG_SIZE = 20;
String logBuffer[LOG_SIZE];
int logIndex = 0;
int logCount = 0;

void scanI2C() {
  i2cCount = 0;
  for (uint8_t addr = 1; addr < 127 && i2cCount < 8; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      i2cDevices[i2cCount++] = addr;
    }
  }
}

void addLogEntry() {
  String entry = String(millis() / 1000);
  entry += ",";
  entry += String(analog0);
  entry += ",";
  entry += String(analog1);
  entry += ",";
  entry += String(digitalState);
  entry += ",";
  entry += String(digitalTriggers);

  logBuffer[logIndex] = entry;
  logIndex = (logIndex + 1) % LOG_SIZE;
  if (logCount < LOG_SIZE) logCount++;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DIGITAL_IN, INPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(9600);
  Wire.begin();

  Bridge.begin();
  server.listenOnLocalhost();
  server.begin();

  scanI2C();

  Serial.println("MultiSensor ready");
  Serial.print("I2C devices found: ");
  Serial.println(i2cCount);

  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  unsigned long now = millis();

  // Read sensors
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;

    analog0 = analogRead(ANALOG_0);
    analog1 = analogRead(ANALOG_1);

    // Debounced digital read
    int reading = digitalRead(DIGITAL_IN);
    if (reading != lastDigital) {
      lastDebounce = now;
    }
    if ((now - lastDebounce) > 50) {
      if (reading != digitalState) {
        digitalState = reading;
        if (digitalState == HIGH) digitalTriggers++;
      }
    }
    lastDigital = reading;

    // Update Bridge
    Bridge.put("a0", String(analog0));
    Bridge.put("a1", String(analog1));
    Bridge.put("d2", String(digitalState));
    Bridge.put("triggers", String(digitalTriggers));
    Bridge.put("uptime", String(now / 1000));
  }

  // Periodic logging
  if (now - lastLog >= LOG_INTERVAL) {
    lastLog = now;
    addLogEntry();

    Serial.print("A0=");
    Serial.print(analog0);
    Serial.print(" A1=");
    Serial.print(analog1);
    Serial.print(" D2=");
    Serial.print(digitalState);
    Serial.print(" T=");
    Serial.println(digitalTriggers);
  }

  // REST API
  BridgeClient client = server.accept();
  if (client) {
    String command = client.readStringUntil('/');
    command.trim();

    if (command == "all") {
      client.print("{\"analog\":{\"a0\":");
      client.print(analog0);
      client.print(",\"a1\":");
      client.print(analog1);
      client.print(",\"a0_v\":");
      client.print(analog0 * 5.0 / 1023.0, 3);
      client.print(",\"a1_v\":");
      client.print(analog1 * 5.0 / 1023.0, 3);
      client.print("},\"digital\":{\"d2\":");
      client.print(digitalState);
      client.print(",\"triggers\":");
      client.print(digitalTriggers);
      client.print("},\"i2c\":{\"devices\":");
      client.print(i2cCount);
      client.print(",\"addresses\":[");
      for (int i = 0; i < i2cCount; i++) {
        if (i > 0) client.print(",");
        client.print("\"0x");
        if (i2cDevices[i] < 16) client.print("0");
        client.print(String(i2cDevices[i], HEX));
        client.print("\"");
      }
      client.print("]},\"uptime_s\":");
      client.print(millis() / 1000);
      client.print("}");
    } else if (command == "analog") {
      client.print("{\"a0\":");
      client.print(analog0);
      client.print(",\"a1\":");
      client.print(analog1);
      client.print(",\"a0_v\":");
      client.print(analog0 * 5.0 / 1023.0, 3);
      client.print(",\"a1_v\":");
      client.print(analog1 * 5.0 / 1023.0, 3);
      client.print("}");
    } else if (command == "digital") {
      client.print("{\"d2\":");
      client.print(digitalState);
      client.print(",\"triggers\":");
      client.print(digitalTriggers);
      client.print("}");
    } else if (command == "log") {
      client.println("time_s,a0,a1,d2,triggers");
      int start = (logCount < LOG_SIZE) ? 0 : logIndex;
      for (int i = 0; i < logCount; i++) {
        int idx = (start + i) % LOG_SIZE;
        client.println(logBuffer[idx]);
      }
    } else if (command == "scan") {
      scanI2C();
      client.print("{\"i2c_devices\":");
      client.print(i2cCount);
      client.print("}");
    } else if (command == "reset") {
      digitalTriggers = 0;
      logCount = 0;
      logIndex = 0;
      client.print("{\"status\":\"counters and log reset\"}");
    } else {
      client.print("{\"error\":\"unknown command. try: all, analog, digital, log, scan, reset\"}");
    }
    client.stop();
  }
}
