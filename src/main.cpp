#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#define CONSOLE 
#define SERIAL_BAUD 115200 
#define WIFI_SSID "Mann im Mond"
#define WIFI_PASSWORD "WarumBinIchSoFroehlich??"
#define IP 66
#define SERVER_PORT 80
#define FILENAME_PARAMETER "filename"

#define NRST 5
#define BOOT0 4
#define BOOT1 2
#define TIMEOUT_MS 1000

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include "stm32ota.h"
#include "esp_task_wdt.h"

#include "esp_log.h"



static const char* TAG = "SERVER";

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
IPAddress ip(192, 168, 178, IP);
IPAddress gateway(192, 168, 178, 1);
IPAddress subnet(255,255,255,0);
AsyncWebServer server(SERVER_PORT);
File uploadFile;

void activateFlashMode() {
  digitalWrite(BOOT0, HIGH);
  digitalWrite(BOOT1, LOW);
}

void shutdown() {
  digitalWrite(NRST, LOW);
}

void startup(){
  digitalWrite(NRST, HIGH);
}

String getFilenameFromRequestParams(AsyncWebServerRequest* request) {
    int paramsCount = request->params();
    int paramsIndex = 0;
    AsyncWebParameter* parameter;
    String filename = "\0";
    while (paramsIndex < paramsCount) {
        parameter = request->getParam(paramsIndex);
        if (parameter->name() == FILENAME_PARAMETER) {
          filename =  parameter->value();
          break;
        }
      paramsIndex++;
    }

    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
          
    return filename;
}

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

void runTarget() {
  digitalWrite(BOOT0, LOW);
  digitalWrite(BOOT1, HIGH);
}

void applyReset(){
  digitalWrite(NRST, LOW);
  delay(10);
  digitalWrite(NRST, HIGH);
}

void connect() {
  runTarget();
  applyReset();
  Serial.println("Disconnected");
  WiFi.setSleep(false);// this code solves my problem
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.println("Connecting");
  }
  Serial.println("Connected");
  activateFlashMode();
  applyReset();
}
void setupWifi() {
  WiFi.mode(WIFI_MODE_STA);
  WiFi.config(ip, gateway, subnet);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  connect(); 
}

void redirectToVersion (AsyncWebServerRequest *request){
  request->redirect("/api/version");
}

void sendVersion(AsyncWebServerRequest *request){
  String content = "<h1>Firmeware version</h1><h2>0.1</h2>";
  request->send(200, "text/html", makePage("Firmware version", content));
} 

void targetVersion(AsyncWebServerRequest *request){
  char targetVersion = stm32Version();
  String content = "<h1>Target firmware version</h1><h2>TODO</h2>";
  request->send(200, "text/html", makePage("Firmware version", content));
}


void restartTarget(AsyncWebServerRequest *request){
  applyReset();
  String content = "<h1>Target Reset</h1><h2>The target has been resetted.</h2>";
  request->send(200, "text/html", makePage("Target Reset", content));
}

void targetRunMode(AsyncWebServerRequest *request){
  runTarget();
  // delay(100);
  // applyReset();
  request->send(200);
}

void targetFlashMode(AsyncWebServerRequest *request){
  activateFlashMode();
  // delay(100);
  // applyReset();
  // delay(500);
  // Serial2.write(STM32INIT);
  // // TODO find out chip name
  request->send(200);
}

int size = 0;
int iteration = 0;
void fileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
  if (!index) {
    String localFileName = filename.startsWith("/") ? filename : "/" + filename;
    uploadFile = SPIFFS.open(localFileName, FILE_WRITE); 
  }

  if (!uploadFile) {
    request->send(404, "text/html", makePage("File not found", ""));
    return;
  }

  Serial.println(String(index) +  " " + String(len));
  size += len;
  iteration++;

  uploadFile.write(data, len);

  if (final) {
    delay(1000);
    uploadFile.close();
    delay(1000);
    String content = "<h1>Upload Complete</h1><br>File size: " + String(uploadFile.size()) + "</br><h2><a style=\"color:white\" href=\"/api/file/select\">Return </a></h2>";
    request->send(200, "text/html", makePage("Upload complete", content));
    Serial2.println("Final - " + String(iteration) + " - " + String(size));
    return;
  }

  
}

void listFiles(AsyncWebServerRequest *request){
  String FileList = "Bootloader Ver: ";
  String Listcode;
  char blversion = 0;
  File root = SPIFFS.open("/");
  if (!root.isDirectory()) {
    return;
  }

  FileList += "<br><br>\n";
  File file = root.openNextFile();
  while (file)
  {
    String FileName = file.name();
    String FileSize = String(file.size());
    Serial2.println("Size: " + FileSize);
    int whsp = 6 - FileSize.length();
    while (whsp-- > 0)
    {
      FileList += " ";
    }
    FileList +=  "File: " + FileName + "   Size:" + FileSize + "\n";
    file = root.openNextFile();
  }
  Listcode = "<h1>List STM32 BinFile</h1><h2>" + FileList + "<br><br><a style=\"color:white\" href=\"/flash\">Flash Menu</a><br><br><a style=\"color:white\" href=\"/delete\">Delete BinFile </a><br><br><a style=\"color:white\" href=\"/api/file/select\">Upload BinFile</a></h2>";
  request->send(200, "text/html", makePage("FileList", Listcode));
}

void selectFile(AsyncWebServerRequest *request){
  char* content = "<h1>Upload STM32 BinFile</h1><h2><br><br><form method='POST' action='/api/file/upload/bin' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Upload'></form></h2>";
  request->send(200, "text/html", makePage("Select file", content));
}

bool awaitData() {
  unsigned long timestamp = millis();
  while(millis() - timestamp <= TIMEOUT_MS) {
    if (Serial2.available()) return true;
  }
  Serial.println("Timeout");
  return false;
}

bool awaitAck() {
  if (!awaitData()) {
    return false;
  }

  char val = Serial2.read();
  Serial.println("Received: " + String(val));
  return val == STM32ACK;
}


bool activateUart() {
  Serial2.write(STM32INIT);
  return awaitAck();
}


void flashTarget(AsyncWebServerRequest *request) {
  
  while(Serial2.available()) {
    Serial2.read();
  }

  activateFlashMode();
  applyReset();
  delay(1000);
  if (!activateUart()) {
    Serial2.println("UART not activated");
    request->send(500, "text/html", makePage("UART failed", "Failed to activate uart for flash"));
    return;
  }

  String filename = getFilenameFromRequestParams(request);
  int filenameLength = filename.length() + 1;
  char filenameArray[filenameLength ];
  filenameArray[filenameLength] = '\0';
  filename.toCharArray(filenameArray, filenameLength);
  File file = SPIFFS.open(filenameArray);
  if (!file) {
    request->send(404, "text/html", makePage("File not found", "File " + filename + " could not be found"));
    file.close();
    return;
  }

  Serial.println("File size:" + String(file.size()));

  uint8_t buffer[256];
  String flashwr;
  bool failure = false;
  unsigned int bufferIterations = file.size() / 256;
  unsigned int lastBufferSize = file.size() % 256;
  Serial.println("Iterations:" + String(bufferIterations) + " | " + String(lastBufferSize));
  request->send(204, "text/html", makePage("Flashing started", "\nFilename: " + filename + "Iterations: " + String(bufferIterations) + "-" + String(lastBufferSize) + "<br>"));
  for (int iteration = 0; iteration < bufferIterations && !failure; iteration++) {

    Serial.println("Iteration " + String(iteration));
    file.read(buffer, 256);
    Serial.println("WR");
    stm32SendCommand(STM32WR);
    if (!awaitAck()) {
      failure = true;
      break;
    }

    Serial.println("ADDR");
    stm32Address(STM32STADDR + (256 * iteration));
    if (!awaitAck()) {
      failure = true;
      break;
    }

    Serial.println("DATA");
    // The argument is soooo wrong!!! It's length -1 to be expressed as char
    stm32SendData(buffer, 255);
    if (!awaitAck()) {
      failure = true;
      break;
    }
    esp_task_wdt_reset();
  }

  Serial.println("Iterations complete");

  if (failure) {
    Serial.println("Failure");
    file.close();
    return;
  }

  Serial.println("WR");
  stm32SendCommand(STM32WR); 
  if (!awaitAck()) {
    Serial.println("Failure");
    file.close();
    return;
  }


  file.read(buffer, lastBufferSize);
  Serial.println("ADDRESS");
  stm32Address(STM32STADDR + (256 * bufferIterations));
  if (!awaitAck()) {
    Serial.println("Failure");
    request->send(500, "text/html", makePage("Flash failed", "Flash failed due to missing ACK when waiting for adress in last iteration"));
    file.close();
    return;
  }

  Serial.println("DATA");
  stm32SendData(buffer, lastBufferSize);
  if (!awaitAck()) {
    Serial.println("Failure");
    request->send(500, "text/html", makePage("Flash failed", "Flash failed due to data of last iteration not confirmed"));
    file.close();
    return;
  }

  Serial.println("Ready");
  file.close();
  Serial.println("File closed");

  delay(1000);

  stm32SendCommand(STM32RUN);
  if (!awaitAck()) {
    return;
  }

  stm32Address(STM32STADDR);
  if (!awaitAck()) {
    //
  }
  // runTarget();
  // applyReset();
  return;

}

void deleteFile(AsyncWebServerRequest *request){

  String filename = String(getFilenameFromRequestParams(request));
  File file = SPIFFS.open(filename);
  if (!file) {
    return request->send(404, "text/html", makePage("File " + filename + "not found", "404"));
  }

  SPIFFS.remove(filename);
  request->send(200, "text/html", makePage("File deleted", "<h2>File " + filename + "has been deleted.<br><br><a style=\"color:white\" href=\"/api/file/list\">Return </a></h2>"));
    
}
void clearStorage(AsyncWebServerRequest *request) {
    File root = SPIFFS.open("/");
    if (!root.isDirectory()) {
      return;
    }

    String deletedFilesLog = "Deleted files:\n";
    File file = root.openNextFile();
    while (file) {
      if (!file) {
        continue;
      }
      deletedFilesLog += String(file.name()) + "\n";
      SPIFFS.remove(file.name());
      file = root.openNextFile();
    }

    request->send(200, "text/html", makePage("Storage cleared", "<h2> Deleted files:\n" + deletedFilesLog + "<br><br><a style=\"color:white\" href=\"/api/file/list\">Return </a></h2>"));
}

void routeNotFound(AsyncWebServerRequest *request){
  request->send(404, "text/html", makePage("Route not found", "404"));
}


void shutdownTarget(AsyncWebServerRequest *request) {
  shutdown();
  request->send(200);
}



void startupTarget(AsyncWebServerRequest *request) {
  startup();
  request->send(200);
}

void sendJsonToTarget(AsyncWebServerRequest *request, JsonVariant &json) {
  JsonObject requestBody = json.as<JsonObject>();
  if (!requestBody.containsKey("length") || !requestBody.containsKey("data")) {
    request->send(500, "text/html", makePage("Missing key", "Missing key"));
  }

  const uint8_t length = requestBody["length"].as<uint8_t>();
  uint8_t bytes[length];
  JsonArray dataArray = requestBody["data"].as<JsonArray>();
  uint8_t index = 0;
  for(JsonVariant v : dataArray) {
    bytes[index++] = (char) v.as<uint8_t>();
  } 

  Serial2.write(bytes, length);
  if (!awaitAck()) {
    request->send(500);
    return;
  }
  request->send(204); 
}

void onRequestBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (request->url() == "/api/target/send") {
    request->send(200);
    return;
  }
}

void eraseTarget(AsyncWebServerRequest *request) {
  stm32SendCommand(STM32ERASEN);
  if (!awaitAck()) {
    request->send(500);
    return;
  }

  // Global erase
  unsigned char globalErase[] = {0xFF, 0xFF};
  stm32SendData(globalErase, 2);
  if (!awaitAck()) {
    request->send(500);
    return;
  }

  request->send(200);
}

void setupServer() {
  server.on("/",  HTTP_GET, redirectToVersion);
  server.on("/api/version", HTTP_GET, sendVersion);
  server.on("/api/file/list", HTTP_GET, listFiles);
  server.on("/api/file/select", HTTP_GET, selectFile);
  server.on("/api/file/delete", HTTP_GET, deleteFile);
  server.on("/api/file/clear", HTTP_GET, clearStorage);
  server.on("/api/file/upload", HTTP_POST, [](AsyncWebServerRequest *request){
  }, fileUpload);
  server.on("/api/target/version", HTTP_GET, targetVersion);
  server.on("/api/target/flash", HTTP_GET, flashTarget);
  server.on("/api/target/erase", HTTP_GET, eraseTarget);
  server.on("/api/target/restart", HTTP_GET, restartTarget);
  server.on("/api/target/mode/flash", HTTP_GET, targetFlashMode);
  server.on("/api/target/mode/run", HTTP_GET, targetRunMode);
  server.on("/api/target/shutdown", HTTP_GET, shutdownTarget);
  server.on("/api/target/startup", HTTP_GET, startupTarget);
  server.addHandler(new AsyncCallbackJsonWebHandler("/api/target/send", sendJsonToTarget) );
  // server.onRequestBody(onRequestBody);
  server.onNotFound(routeNotFound);
  server.begin();
}

void setupGPIO() {
  pinMode(BOOT0, OUTPUT);
  pinMode(BOOT1, OUTPUT);
  pinMode(NRST, OUTPUT);

  delay(100);
  runTarget();
  delay(100);
  digitalWrite(NRST, LOW);
  delay(50);
  digitalWrite(NRST, HIGH);
  delay(500);
}

void setupFileSystem() {
  if (!SPIFFS.begin(true)) {
    return;
  }  
}

void setup() {
  esp_log_level_set("*", ESP_LOG_ERROR);        // set all components to ERROR level
  esp_log_level_set("wifi", ESP_LOG_WARN);      // enable WARN logs from WiFi stack
  esp_log_level_set("dhcpc", ESP_LOG_INFO);     // enable INFO logs from DHCP client

  Serial2.begin(SERIAL_BAUD, SERIAL_8E1);
  Serial.begin(SERIAL_BAUD);

  setupWifi();
  setupServer();
  setupGPIO();
  setupFileSystem();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connect();
  }
}