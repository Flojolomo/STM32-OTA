#include <Arduino.h>

#define SERIAL_BAUD 115200
#define WIFI_SSID "Mann im Mond"
#define WIFI_PASSWORD "WarumBinIchSoFreohlich??"

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println("Hello");
  // put your setup code here, to run once:
}

void loop() {
  delay(1000);
  Serial.println("Still running ...");
  // put your main code here, to run repeatedly:
}