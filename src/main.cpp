#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include "stm32ota.h"

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

void setupWifi() {
  uint8_t dotCount = 0;
  wl_status_t status = WiFi.status();
        WiFi.begin(ssid, password);
  do {
    delay(500);
    for (uint8_t count = 0; count < dotCount; count++) {
    }

    if (dotCount > 5) {
      dotCount = 0;
    } else {
      dotCount++;
    }
    status = WiFi.status();
  } while (status != WL_CONNECTED);
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

void applyReset(){
  digitalWrite(NRST, LOW);
  delay(10);
  digitalWrite(NRST, HIGH);
}

void restartTarget(AsyncWebServerRequest *request){
  applyReset();
  String content = "<h1>Target Reset</h1><h2>The target has been resetted.</h2>";
  request->send(200, "text/html", makePage("Target Reset", content));
}

void runTarget() {
  digitalWrite(BOOT0, LOW);
  digitalWrite(BOOT1, HIGH);
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
  // Serial.write(STM32INIT);
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
    Serial.println("Final - " + String(iteration) + " - " + String(size));
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
    Serial.println("Size: " + FileSize);
    int whsp = 6 - FileSize.length();
    while (whsp-- > 0)
    {
      FileList += " ";
    }
    FileList +=  "File: " + FileName + "   Size:" + FileSize + "\n";
    file = root.openNextFile();
  }
  Listcode = "<h1>List STM32 BinFile</h1><h2>" + FileList + "<br><br><a style=\"color:white\" href=\"/flash\">Flash Menu</a><br><br><a style=\"color:white\" href=\"/delete\">Delete BinFile </a><br><br><a style=\"color:white\" href=\"/up\">Upload BinFile</a></h2>";
  request->send(200, "text/html", makePage("FileList", Listcode));
}

void selectFile(AsyncWebServerRequest *request){
  char* content = "<h1>Upload STM32 BinFile</h1><h2><br><br><form method='POST' action='/api/file/upload/bin' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Upload'></form></h2>";
  request->send(200, "text/html", makePage("Select file", content));
}

bool awaitData() {
  unsigned long timestamp = millis();
  while(millis() - timestamp <= TIMEOUT_MS) {
    if (Serial.available()) return true;
  }
  return false;
}

bool awaitAck() {
  if (!awaitData()) {
    return false;
  }

  return Serial.read() == STM32ACK;
}


bool activateUart() {
  Serial.write(STM32INIT);
  return awaitAck();
}


void flashTarget(AsyncWebServerRequest *request) {
  
  while(Serial.available()) {
    Serial.read();
  }

  activateFlashMode();
  applyReset();
  delay(1000);
  if (!activateUart()) {
    Serial.println("UART not activated");
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

  uint8_t buffer[256];
  String flashwr;
  bool failure = false;
  unsigned int bufferIterations = file.size() / 256;
  unsigned int lastBufferSize = file.size() % 256;
  
  for (int iteration = 0; iteration < bufferIterations && !failure; iteration++) {

    file.read(buffer, 256);
    stm32SendCommand(STM32WR);
    if (!awaitAck()) {
      failure = true;
      break;
    }

    stm32Address(STM32STADDR + (256 * iteration));
    if (!awaitAck()) {
      Serial.println("Failure at adress");
      failure = true;
      break;
    }

    // The argument is soooo wrong!!! It's length -1 to be expressed as char
    stm32SendData(buffer, 255);
    if (!awaitAck()) {
      failure = true;
      break;
    }
  }

  if (failure) {
    request->send(500, "text/html", makePage("Flash failed", "Flash failed due to missing ACK in iteration " + String(iteration) + " of " + String(bufferIterations)));
    file.close();
    return;
  }

  stm32SendCommand(STM32WR); 
  if (!awaitAck()) {
    request->send(500, "text/html", makePage("Flash failed", "Flash failed due to missing ACK in last iteration"));
    file.close();
    return;
  }

  file.read(buffer, lastBufferSize);
  stm32Address(STM32STADDR + (256 * bufferIterations));
  if (!awaitAck()) {
    request->send(500, "text/html", makePage("Flash failed", "Flash failed due to missing ACK when waiting for adress in last iteration"));
    file.close();
    return;
  }

  stm32SendData(buffer, lastBufferSize);
  if (!awaitAck()) {
    request->send(500, "text/html", makePage("Flash failed", "Flash failed due to data of last iteration not confirmed"));
    file.close();
    return;
  }

  file.close();
  request->send(200, "text/html", makePage("Read complete", "\nFilename: " + filename + "Iterations: " + String(bufferIterations) + "-" + String(lastBufferSize) + "<br>"));
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

void shutdown() {
  digitalWrite(NRST, LOW);
}

void shutdownTarget(AsyncWebServerRequest *request) {
  shutdown();
  request->send(200);
}

void startup(){
  digitalWrite(NRST, HIGH);
}

void startupTarget(AsyncWebServerRequest *request) {
  startup();
  request->send(200);
}

boolean sendJsonToTarget(AsyncWebServerRequest *request, JsonVariant &json) {
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

  Serial.write(bytes, length);
  while (!Serial.available());
  request->send(Serial.read() == STM32ACK ? 204 : 500);
  
  
  // char * p_Byte = dataArray.begin();
  // for (uint8_t index = 0; index < length; index++) {
    
  // }
  // Serial.printf("#### %d\n", length);
  // serializeJson(requestBody["data"], Serial);
  
  // Serial.printf("####");
  // serializeJson(bytes, Serial);
  // // const std::vector<uint8_t> data = requestBody["data"].as<std::vector<uint8_t>>();
}

void sendToTarget(uint8_t *data, size_t len, size_t index, size_t total) {
  // StaticJsonBuffer<50> JSONBuffer; 
  // JsonObject& parsed = JSONBuffer.parseObject(data);

  // if (!parsed.success()) {   //Check for errors in parsing
  //   Serial.println("Parsing failed");
  //   return;
  // }
  
  // if(!index){
  //   Serial.printf("BodyStart: %u B\n", total);
  // }
  // for(size_t i=0; i<len; i++){
  //   Serial.write(data[i]);
  // }
  // if(index + len == total){
  //   Serial.printf("BodyEnd: %u B\n", total);
  // }
}

void onRequestBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (request->url() == "/api/target/send") {
    request->send(200);
    return;
  }
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
  Serial.begin(SERIAL_BAUD, SERIAL_8E1);

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
    shutdown();
    setupWifi();
  }
  // put your main code here, to run repeatedly:
}