#include <Arduino.h>
#include <WiFi.h>

#define SERIAL_BAUD 115200
#define WIFI_SSID 
#define WIFI_PASSWORD 

const char* ssid = "Mann im Mond";
const char* password = "WarumBinIchSoFroehlich??";

void setupWifi() {
  uint8_t dotCount = 0;
  wl_status_t status = WL_DISCONNECTED;
  do {
    switch (status) {
      case WL_DISCONNECTED:
      case WL_CONNECT_FAILED:
        Serial.println("Reconnect");
        WiFi.begin(ssid, password);
      default:
        break;
    }
    delay(500);
    Serial.print("connecting .");
    for (uint8_t count = 0; count < dotCount; count++) {
      Serial.print(".");
    }
    Serial.println("");

    if (dotCount > 5) {
      dotCount = 0;
    } else {
      dotCount++;
    }
    status = WiFi.status();
    Serial.print(status);
  } while (status != WL_CONNECTED);
 
  Serial.print("Connected - IP:");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println("Hello");
  // put your setup code here, to run once:
}

void loop() {
  delay(1000);
  Serial.println("Still running ...");
  if (WiFi.status() != WL_CONNECTED) {
    setupWifi();
  }
  // put your main code here, to run repeatedly:
}