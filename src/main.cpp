#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include "stm32ota.h"

#define SERIAL_BAUD 115200
#define WIFI_SSID "Mann im Mond"
#define WIFI_PASSWORD "WarumBinIchSoFroehlich??"
#define IP 66
#define SERVER_PORT 80

#define NRST 5
#define BOOT0 4
#define LED 2

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
IPAddress ip(192, 168, 178, IP);
IPAddress gateway(192, 168, 178, 1);
IPAddress subnet(255,255,255,0);
AsyncWebServer server(SERVER_PORT);


String makePage(String title, String contents) {
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<title >";
  s += title;
  s += "</title></head><body text=#ffffff bgcolor=##4da5b9 align=\"center\">";
  s += contents;
  s += "</body></html>";
  return s;
}

void setupWifi() {
  uint8_t dotCount = 0;
  wl_status_t status = WiFi.status();
        WiFi.begin(ssid, password);
  do {
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

void redirectToVersion (AsyncWebServerRequest *request) {
  request->redirect("/api/version");
}

void sendVersion(AsyncWebServerRequest *request) {
  String content = "<h1>Firmeware version</h1><h2>0.1</h2>";
  request->send(200, "text/html", makePage("Firmware version", content));
} 

void targetVersion(AsyncWebServerRequest *request) {
  char targetVersion = stm32Version();
  String content = "<h1>Target firmware version</h1><h2>TODO</h2>";
  request->send(200, "text/html", makePage("Firmware version", content));
}

void applyReset() {
  digitalWrite(NRST, LOW);
  delay(100);
  digitalWrite(NRST, HIGH);
}
void restartTarget(AsyncWebServerRequest *request) {
  applyReset();
  String content = "<h1>Target Reset</h1><h2>The target has been resetted.</h2>";
  request->send(200, "text/html", makePage("Target Reset", content));
}

void targetRunMode(AsyncWebServerRequest *request) {
  digitalWrite(BOOT0, LOW);
  delay(100);
  applyReset();
  request->send(200);
}

void targetFlashMode(AsyncWebServerRequest *request) {
  digitalWrite(BOOT0, HIGH);
  delay(100);
  applyReset();
  request->send(200);
}

void fileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (final) {
    String content = "<h1>Upload Complete</h1><h2><a style=\"color:white\" href=\"/api/file/upload/select\">Return </a></h2>";
    request->send(200, "text/html", makePage("Upload complete", content));
    return;
  }

  Serial.println("Upload file" + filename);
  Serial.print("Size ");
  Serial.println(len);

  String localFileName = filename.startsWith("/") ? filename : "/" + filename;
  Serial.println(localFileName);
  File file = SPIFFS.open(localFileName, FILE_WRITE);
  if (!file) {
    Serial.println("Write failed");
    return;
  }

  file.write(data, len);
  file.close();
  Serial.println("Write succeeded");

  // Read file
  Serial.println("Read file");
  File readFile = SPIFFS.open(localFileName, FILE_READ);
  if (!readFile) {
    Serial.println("Read failed");
    return;
  }

  while(readFile.available()){
    Serial.write(readFile.read());
  }
  Serial.println("readFile end");
  readFile.close();
}

void listFiles(AsyncWebServerRequest *request){
  Serial.println("List files");
  String FileList = "Bootloader Ver: ";
  String Listcode;
  char blversion = 0;
  File root = SPIFFS.open("/");
  Serial.println("File root is " + root.isDirectory() ? "a directory" : "a file");
  if (!root.isDirectory()) {
    Serial.println("Return after directory check");
    return;
  }
  // FileList = "<br> MCU: ";
  // blversion = stm32Version();
  // FileList += String((blversion >> 4) & 0x0F) + "." + String(blversion & 0x0F) + "<br> MCU: ";
  // FileList += STM32_CHIPNAME[stm32GetId()];
  FileList += "<br><br> File: ";
  File file = root.openNextFile();
  Serial.println(file.name());
  while (file)
  {
    Serial.print("Found file ");
    Serial.println(file.name());
    String FileName = file.name();
    String FileSize = String(file.size());
    int whsp = 6 - FileSize.length();
    while (whsp-- > 0)
    {
      FileList += " ";
    }
    FileList +=  FileName + "   Size:" + FileSize;
    file = root.openNextFile();
  }
  Listcode = "<h1>List STM32 BinFile</h1><h2>" + FileList + "<br><br><a style=\"color:white\" href=\"/flash\">Flash Menu</a><br><br><a style=\"color:white\" href=\"/delete\">Delete BinFile </a><br><br><a style=\"color:white\" href=\"/up\">Upload BinFile</a></h2>";
  request->send(200, "text/html", makePage("FileList", Listcode));
}

void routeNotFound(AsyncWebServerRequest *request) {
  request->send(404, "text/html", makePage("Route not found", "404"));
}

void setupServer() {
  server.on("/",  HTTP_GET, [](AsyncWebServerRequest *request){
    redirectToVersion(request);
  });
  server.on("/api/version", HTTP_GET, [](AsyncWebServerRequest *request){
    sendVersion(request);
  });
  server.on("/api/file/select", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(501);
  });
  server.on("/api/file/list", HTTP_GET, listFiles);
  server.on("/api/file/upload/select", HTTP_GET, [](AsyncWebServerRequest *request){
    char* content = "<h1>Upload STM32 BinFile</h1><h2><br><br><form method='POST' action='/api/file/upload/bin' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Upload'></form></h2>";
    request->send(200, "text/html", makePage("Select file", content));
  });
  server.on("/api/file/upload/bin", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("Upload file");
    // request->send(200, "text/html", makePage("FileList", "<h1> Uploaded OK </h1><br><br><h2><a style=\"color:white\" href=\"/list\">Return </a></h2>"));
    request->send(200);
  }, fileUpload);
  server.on("/api/target/version", HTTP_GET, [](AsyncWebServerRequest *request){
    targetVersion(request);
  });
  server.on("/api/target/flash", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(501);
  });
  server.on("/api/target/restart", HTTP_GET, [](AsyncWebServerRequest *request){
    restartTarget(request);
  });
  server.on("/api/target/mode/flash", HTTP_GET, [](AsyncWebServerRequest *request){
    targetFlashMode(request);
  });
  server.on("/api/target/mode/run", HTTP_GET, [](AsyncWebServerRequest *request){
    targetRunMode(request);
  });

  
  

  server.onNotFound([](AsyncWebServerRequest *request){
    routeNotFound(request);
  });
  server.begin();
}

void setupGPIO() {
  pinMode(BOOT0, OUTPUT);
  pinMode(NRST, OUTPUT);
  pinMode(LED, OUTPUT);

  delay(100);
  digitalWrite(BOOT0, HIGH);
  delay(100);
  digitalWrite(NRST, LOW);
  digitalWrite(LED, LOW);
  delay(50);
  digitalWrite(NRST, HIGH);
  delay(500);
}

void setupFileSystem() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }  
}

void setup() {
  Serial.begin(SERIAL_BAUD, SERIAL_8E1);
  Serial.println("Hello");

  WiFi.mode(WIFI_MODE_STA);
  WiFi.config(ip, gateway, subnet);

  setupServer();
  setupGPIO();
  setupFileSystem();
  // put your setup code here, to run once:
}

void loop() {
  delay(1000);
  if (WiFi.status() != WL_CONNECTED) {
    setupWifi();
  }
  // put your main code here, to run repeatedly:
}