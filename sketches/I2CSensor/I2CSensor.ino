/*
  I2CSensor - Arduino Yun
  Generic I2C sensor scanner and reader. Starts by scanning the bus to find
  connected devices, then reads from a configured address.

  Works out of the box as a bus scanner. Edit TARGET_ADDR and the
  readSensor() function for your specific I2C sensor (BME280, BMP180,
  MPU6050, ADS1115, SHT31, etc.)

  Wiring:
    SDA -> SDA (pin 2 on Yun)
    SCL -> SCL (pin 3 on Yun)
    VCC -> 3.3V or 5V (check sensor datasheet)
    GND -> GND

  Access readings via: http://192.168.11.36/arduino/i2c
*/

#include <Wire.h>
#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>

BridgeServer server;

// Set to your sensor's I2C address (0x00 = scan only mode)
const uint8_t TARGET_ADDR = 0x00;
const unsigned long READ_INTERVAL = 2000;

uint8_t foundDevices[16];
int deviceCount = 0;
unsigned long lastRead = 0;
int rawBytes[6];
int bytesRead = 0;

void scanBus() {
  deviceCount = 0;
  Serial.println("Scanning I2C bus...");

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      if (deviceCount < 16) {
        foundDevices[deviceCount] = addr;
        deviceCount++;
      }
      Serial.print("  Found device at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
    }
  }

  Serial.print("Scan complete. ");
  Serial.print(deviceCount);
  Serial.println(" device(s) found.");

  Bridge.put("i2c_devices", String(deviceCount));
}

// Reads raw bytes from the target sensor.
// Customize this for your specific sensor's register map.
void readSensor(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(0x00);  // Starting register (change per sensor)
  Wire.endTransmission(false);

  bytesRead = 0;
  Wire.requestFrom(addr, (uint8_t)6);
  while (Wire.available() && bytesRead < 6) {
    rawBytes[bytesRead] = Wire.read();
    bytesRead++;
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(9600);
  Wire.begin();

  Bridge.begin();
  server.listenOnLocalhost();
  server.begin();

  scanBus();

  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("I2CSensor ready");
}

void loop() {
  unsigned long now = millis();

  if (now - lastRead >= READ_INTERVAL) {
    lastRead = now;

    uint8_t addr = TARGET_ADDR;
    if (addr == 0x00 && deviceCount > 0) {
      addr = foundDevices[0];  // Auto-use first found device
    }

    if (addr != 0x00) {
      readSensor(addr);

      String hexStr = "";
      for (int i = 0; i < bytesRead; i++) {
        if (rawBytes[i] < 16) hexStr += "0";
        hexStr += String(rawBytes[i], HEX);
        if (i < bytesRead - 1) hexStr += " ";
      }
      Bridge.put("i2c_addr", String(addr, HEX));
      Bridge.put("i2c_raw", hexStr);
      Bridge.put("i2c_bytes", String(bytesRead));

      Serial.print("0x");
      Serial.print(addr, HEX);
      Serial.print(": ");
      Serial.println(hexStr);
    }
  }

  BridgeClient client = server.accept();
  if (client) {
    String command = client.readStringUntil('/');
    command.trim();

    if (command == "i2c") {
      client.print("{\"devices\":");
      client.print(deviceCount);
      client.print(",\"addresses\":[");
      for (int i = 0; i < deviceCount; i++) {
        if (i > 0) client.print(",");
        client.print("\"0x");
        if (foundDevices[i] < 16) client.print("0");
        client.print(String(foundDevices[i], HEX));
        client.print("\"");
      }
      client.print("],\"last_read\":{\"addr\":\"0x");
      uint8_t addr = TARGET_ADDR ? TARGET_ADDR : (deviceCount > 0 ? foundDevices[0] : 0);
      if (addr < 16) client.print("0");
      client.print(String(addr, HEX));
      client.print("\",\"bytes\":");
      client.print(bytesRead);
      client.print(",\"raw\":[");
      for (int i = 0; i < bytesRead; i++) {
        if (i > 0) client.print(",");
        client.print(rawBytes[i]);
      }
      client.print("]}}");
    } else if (command == "scan") {
      scanBus();
      client.print("{\"status\":\"rescan complete\",\"devices\":");
      client.print(deviceCount);
      client.print("}");
    } else {
      client.print("{\"error\":\"unknown command\"}");
    }
    client.stop();
  }
}
