#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <AnimatedGIF.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32Ping.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>

#define DEFAULT_SSID "MY_SSID"
char ssid[80] = DEFAULT_SSID;

#define DEFAULT_PASSWORD "password"
char password[80] = DEFAULT_PASSWORD;

#define DEFAULT_AP "rgbmatrix"
char ap[80] = DEFAULT_AP;

#define DEFAULT_AP_PASSWORD "password"
char ap_password[80] = DEFAULT_PASSWORD;

#define DEFAULT_HOSTNAME "rgbmatrix"
char hostname[80] = DEFAULT_HOSTNAME;

#define DEFAULT_BRIGHTNESS 255
const uint8_t brightness = DEFAULT_BRIGHTNESS;

#define DEFAULT_PING_FAIL_COUNT 6 // Set to '0' to disable client check
int ping_fail_count = DEFAULT_PING_FAIL_COUNT;

#define DBG_OUTPUT_PORT Serial

// SD Card reader pins
// ESP32-Trinity Pins
#define SD_SCLK 33
#define SD_MISO 32
#define SD_MOSI 21
#define SD_SS 22

SPIClass spi = SPIClass(HSPI);

// Matrix Config
// See the "displaySetup" method for more display config options
MatrixPanel_I2S_DMA *dma_display = nullptr;
const int panelResX = 64;        // Number of pixels wide of each INDIVIDUAL panel module.
const int panelResY = 32;        // Number of pixels tall of each INDIVIDUAL panel module.
const int panels_in_X_chain = 2; // Total number of panels in X
const int panels_in_Y_chain = 1; // Total number of panels in Y
const int totalWidth  = panelResX * panels_in_X_chain;
const int totalHeight = panelResY * panels_in_Y_chain;
int16_t xPos = 0, yPos = 0; // Top-left pixel coord of GIF in matrix space

WebServer server(80);
IPAddress my_ip;
IPAddress no_ip(0,0,0,0);
IPAddress client_ip(0,0,0,0);

// Style for HTML pages
String style =
  "<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
  "input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
  "#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
  "#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
  "form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
  ".btn{background:#3498db;color:#fff;cursor:pointer}.btn:disabled,.btn.disabled {background:#ddd;color:#fff;cursor:not-allowed;pointer-events: none}"
  ".otabtn{background:#218838;color:#fff;cursor:pointer}.btn.disabled {background:#ddd;color:#fff;cursor:not-allowed;pointer-events: none}"
  ".rebootbtn{background:#c82333;color:#fff;cursor:pointer}.btn.disabled {background:#ddd;color:#fff;cursor:not-allowed;pointer-events: none}"
  "input[type=\"checkbox\"]{margin:0px;width:22px;height:22px;}"
  "</style>";

const char *secrets_filename = "/secrets.json";
const char *gif_filename = "/temp.gif";

String wifi_mode = "AP";
String sd_status = "";
bool card_mounted = false;
bool config_display_on = true;
unsigned long last_seen, start_tick;
int ping_fail = 0;
String sd_filename = "";
File gif_file, upload_file;
String new_gif = "";
String current_gif = "";
AnimatedGIF gif;

void setup(void) {    
  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.setDebugOutput(true);
  delay(1000);

  // Initialize internal filesystem
  DBG_OUTPUT_PORT.println("Loading LITTLEFS");
  if(!LittleFS.begin(true)){
    DBG_OUTPUT_PORT.println("LITTLEFS Mount Failed");
  }
  LittleFS.remove(gif_filename);

  // Initialize SD Card
  spi.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_SS);
  if (!SD.begin(SD_SS, spi, 80000000)) {
    DBG_OUTPUT_PORT.println("Card Mount Failed");
    sd_status = "Mount Failed";
  } else {
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      DBG_OUTPUT_PORT.println("No SD card");
      sd_status = "No card";
    } else {
      card_mounted = true;
      DBG_OUTPUT_PORT.print("SD Card Type: ");
      if (cardType == CARD_MMC) {
        DBG_OUTPUT_PORT.println("MMC");
      } else if (cardType == CARD_SD) {
        DBG_OUTPUT_PORT.println("SDSC");
      } else if (cardType == CARD_SDHC) {
        DBG_OUTPUT_PORT.println("SDHC");
      } else {
        DBG_OUTPUT_PORT.println("UNKNOWN");
      }
      uint64_t cardSize = SD.cardSize() / (1024 * 1024);
      DBG_OUTPUT_PORT.printf("SD Card Size: %lluMB\n", cardSize);
      sd_status = String(cardSize) + "MB";
    }
  }

  // Initialize gif object
  gif.begin(LITTLE_ENDIAN_PIXELS);

  // Initialize Display
  displaySetup();

  // Initialize Wifi
  parseSecrets();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  DBG_OUTPUT_PORT.print("Connecting to ");
  DBG_OUTPUT_PORT.println(ssid);
    
  // Wait for connection
  uint8_t i = 0;
  while ((WiFi.status() != WL_CONNECTED) && (i++ < 20)) { //wait 10 seconds
    delay(500);
  }
  if (i == 21) {
    DBG_OUTPUT_PORT.print("Could not connect to ");
    DBG_OUTPUT_PORT.println(ssid);
  
    // Startup Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap, ap_password);
    my_ip = WiFi.softAPIP();
    DBG_OUTPUT_PORT.print("IP address: ");
    DBG_OUTPUT_PORT.println(my_ip.toString());
  } else {
    my_ip = WiFi.localIP();
    wifi_mode = "Client";
    DBG_OUTPUT_PORT.print("Connected! IP address: ");
    DBG_OUTPUT_PORT.println(my_ip.toString());
  }

  String display_string = "rgbmatrix.local\n" + my_ip.toString() + "\nWifi: " + wifi_mode + "\nSD: " + sd_status;
  dma_display->setCursor(0, 0);
  dma_display->println(display_string);

  // Startup MDNS 
  if (MDNS.begin(hostname)) {
    MDNS.addService("http", "tcp", 80);
    DBG_OUTPUT_PORT.println("MDNS responder started");
    DBG_OUTPUT_PORT.print("You can now connect to http://");
    DBG_OUTPUT_PORT.print(hostname);
    DBG_OUTPUT_PORT.println(".local");
  }

  server.on("/", handleRoot);
  server.on("/ota", handleOTA);
  server.on("/update", HTTP_POST, [](){
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "OTA Update Failure" : "OTA Update Success");
    delay(2000);
    ESP.restart();
  }, handleUpdate);
  server.on("/sdcard", handleSD);
  server.on("/upload", HTTP_POST, [](){server.sendHeader("Connection", "close");}, handleUpload);
  server.on("/localplay", handleLocalPlay);
  server.on("/remoteplay", HTTP_POST, [](){server.send(200);}, handleRemotePlay);
  server.on("/clear", handleClear);
  server.on("/reboot", handleReboot);
  server.onNotFound(handleNotFound);
  server.begin();
  
  start_tick = millis();
  DBG_OUTPUT_PORT.println("Startup Complete");
} /* setup() */

void loop(void) {
  server.handleClient();
  delay(2);
  checkSerialClient();
  // Clear initial boot display after 1min
  if (config_display_on && (millis() - start_tick >= 60*1000UL)){
    config_display_on = false;
    dma_display->clearScreen();
  }
  // Check if client who requested core image has gone away, if so clear screen
  if(client_ip != no_ip){
    if (ping_fail_count != 0){
      if (ping_fail == 0){
        if (millis() - last_seen >= 30*1000UL){
          last_seen = millis();
          bool success = Ping.ping(client_ip, 1);
          if(!success){
            DBG_OUTPUT_PORT.println("Initial Ping failed");
            ping_fail = ping_fail + 1;
          } else {
            DBG_OUTPUT_PORT.println("Ping success");
            ping_fail = 0;
          }
        }
      } else if (ping_fail >= 1 && ping_fail < ping_fail_count){ // Increase ping frequecy after first failure
        if (millis() - last_seen >= 10*1000UL){
          last_seen = millis();
          bool success = Ping.ping(client_ip, 1);
          if(!success){
            DBG_OUTPUT_PORT.print("Ping fail count: ");
            ping_fail = ping_fail + 1;
            DBG_OUTPUT_PORT.println(ping_fail);
          } else {
            DBG_OUTPUT_PORT.println("Ping success");
            ping_fail = 0;
          }
        }
      } else if (ping_fail >= ping_fail_count){
        // Client gone clear display
        DBG_OUTPUT_PORT.println("Client gone, clearing display and deleting the GIF.");
        dma_display->clearScreen();
        client_ip = {0,0,0,0};
        LittleFS.remove(gif_filename);
        sd_filename = "";
      }
    }
  }
} /* loop() */

bool parseSecrets() {
  // Open file for parsing
  File secretsFile = LittleFS.open(secrets_filename);
  if (!secretsFile) {
    DBG_OUTPUT_PORT.println("ERROR: Could not open secrets.json file for reading!");
    return false;
  }

  // Check if we can deserialize the secrets.json file
  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, secretsFile);
  if (err) {
    DBG_OUTPUT_PORT.println("ERROR: deserializeJson() failed with code ");
    DBG_OUTPUT_PORT.println(err.c_str());

    return false;
  }

  // Read settings from secrets.json file
  strlcpy(ssid, doc["ssid"] | DEFAULT_SSID, sizeof(ssid));
  strlcpy(password, doc["password"] | DEFAULT_PASSWORD, sizeof(password));
       
  // Close the secrets.json file
  secretsFile.close();
  return true;
} /* parseSecrets() */

void returnHTML(String html) {
  server.send(200, F("text/html"), html);
} /* returnHTML() */

void returnHTTPError(int code, String msg) {
  server.send(code, F("text/plain"), msg + "\r\n");
} /* returnHTTPError() */

void handleRoot() {
  String gif_button = "";
  if(card_mounted){
    gif_button = "<input type=\"button\" class=btn onclick=\"location.href='/sdcard';\" value=\"GIF Upload\" />";
  }
  String image_status = "";
  if (LittleFS.exists(gif_filename)){
    image_status = "Client IP: " + client_ip.toString() + "<br><br>" +
    "Current Image<br><img src=\"/core.gif\"><img><br>";
  } else if (sd_filename != ""){
    image_status = "Client IP: " + client_ip.toString() + "<br><br>" +
    "Current Image<br><img src=\"" + sd_filename + "\"><img><br>";
  }
  String html =
    "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
    "<head>"
    "<title>web2rgbmatrix</title>"
    "<meta http-equiv=\"refresh\" content=\"30\">"
    "<script>"
    "function myFunction() {"
    "var x = document.getElementById(\"idPassword\")\;"
    "if (x.type === \"password\") {"
    "x.type = \"text\"\;"
    "} else {"
    "x.type = \"password\"\;"
    "}"
    "}"
    "</script>" 
    + style +
    "</head>"
    "<body>"
    "<form action=\"/\">"
    "<h1>web2rgbmatrix</h1><br>"
    "<p>"
    "<b>Status</b><br>"
    "SD Card: " + sd_status + "<br>"
    "Wifi Mode: " + wifi_mode + "<br>"
    "rgbmatrix IP: " + my_ip.toString() + "<br>"
    + image_status + "<br>"
    "<p>"
    "<b>Wifi Client Settings</b><br>"
    "SSID<br>"
    "<input type=\"text\" name=\"ssid\" value=\"" + String(ssid) + "\"><br>"
    "Password<br>"
    "<input type=\"password\" name=\"password\" id=\"idPassword\" value=\"" + String(password) + "\"><br>"
    "<div>"
    "<label for=\"showpass\" class=\"chkboxlabel\">"
    "<input type=\"checkbox\"id=\"showpass\" onclick=\"myFunction()\">"
    " Show Password</label>"
    "</div>"
    "<input type=\"submit\" class=btn value=\"Save\"><br>"
    "</form>"
    "<form>"
    "<input type=\"button\" class=btn onclick=\"location.href='/clear';\" value=\"Clear Display\" />"
    + gif_button +
    "<input type=\"button\" class=otabtn onclick=\"location.href='/ota';\" value=\"OTA Update\" />"
    "<input type=\"button\" class=rebootbtn onclick=\"location.href='/reboot';\" value=\"Reboot\" />"
    "</form>"
    "</body>"
    "</html>";
  if (server.method() == HTTP_GET) {
    if (server.arg("ssid") != "" && server.arg("password") != "") {
      server.arg("ssid").toCharArray(ssid, sizeof(ssid));
      server.arg("password").toCharArray(password, sizeof(password));
      // Write secrets.json
      StaticJsonDocument<200> doc;
      doc["ssid"] = server.arg("ssid");
      doc["password"] = server.arg("password");
      File data_file = LittleFS.open(secrets_filename, FILE_WRITE);
      if (!data_file) {
        returnHTTPError(500, "Failed to open config file for writing");
      }
      serializeJson(doc, data_file);
      html =
        "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
        "<head>"
        "<title>Config Saved</title>"
        "<meta http-equiv=\"refresh\" content=\"3\;URL=\'/\'\" />"
        + style +
        "</head>"
        "<body>"
        "<form>"
        "<p>Config Saved</p>"
        "</form>"
        "</body>"
        "</html>";
    }
    returnHTML(html);
  } else {
    returnHTTPError(405, "SD Card Not Mounted");
  }
} /* handleRoot() */

void handleOTA(){
  if (server.method() == HTTP_GET) {
    String html = 
      "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
      "<head>"
      "<title>web2rgbmatrix - Update</title>"
      + style +
      "</head>"
      "<body>"
      "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
      "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
      "<h1>OTA Update</h1>"
      "<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
      "<label id='file-input' for='file'>   Choose file...</label>"
      "<input id='sub-button' type='submit' class=btn value='Update'>"
      "<br><br>"
      "<div id='prg'></div>"
      "<br><div id='prgbar'><div id='bar'></div></div><br>"
      "<input id='back-button' type=\"button\" class=btn onclick=\"location.href='/';\" value=\"Back\" />"
      "</form>"
      "<script>"
      "function sub(obj){"
      "var fileName = obj.value.split('\\\\');"
      "document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
      "};"
      "$('form').submit(function(e){"
      "e.preventDefault();"
      "var form = $('#upload_form')[0];"
      "var data = new FormData(form);"
      "$.ajax({"
      "url: '/update',"
      "type: 'POST',"
      "data: data,"
      "contentType: false,"
      "processData:false,"
      "xhr: function() {"
      "$('#sub-button').prop('disabled', true);"
      "$('#back-button').prop('disabled', true);"
      "var xhr = new window.XMLHttpRequest();"
      "xhr.upload.addEventListener('progress', function(evt) {"
      "if (evt.lengthComputable) {"
      "var per = evt.loaded / evt.total;"
      "if (evt.loaded != evt.total) {"
      "$('#prg').html('Upload Progress: ' + Math.round(per*100) + '%');"
      "} else {"
      "$('#prg').html('Applying update and rebooting');"
      "}"
      "$('#bar').css('width',Math.round(per*100) + '%');"
      "}"
      "}, false);"
      "return xhr;"
      "},"
      "success:function(d, s) {"
      "console.log('success!');"
      "setTimeout(function(){window.location.href = '/';}, 5000);"
      "},"
      "error: function (a, b, c) {"
      "console.log('ERROR!');"
      "}"
      "});"
      "});"
      "</script>"
      "</body>"
      "</html>";
    returnHTML(html);
  } else {
    returnHTTPError(405, "SD Card Not Mounted");
  }
} /* handleOTA() */

void handleUpdate(){
  client_ip = {0,0,0,0};
  LittleFS.remove(gif_filename);
  sd_filename = "";
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    dma_display->clearScreen();
    dma_display->setCursor(0, 0);
    dma_display->println("OTA Update Started");
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // Start with max available size
      Update.printError(DBG_OUTPUT_PORT);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Flashing firmware to ESP32
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(DBG_OUTPUT_PORT);
      dma_display->clearScreen();
      dma_display->setCursor(0, 0);
      dma_display->println("OTA Update Error");
    } else {
      dma_display->clearScreen();
      dma_display->setCursor(0, 0);
      dma_display->printf("OTA Progress: %d%%\n", (Update.progress()*100)/Update.size());
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      dma_display->clearScreen();
      dma_display->setCursor(0, 0);
      dma_display->printf("OTA Success: %u\nRebooting...\n", upload.totalSize);
    } else {
      Update.printError(DBG_OUTPUT_PORT);
      dma_display->clearScreen();
      dma_display->setCursor(0, 0);
      dma_display->println("OTA Update Error");
    }
  }
} /* handleUpdate() */

void handleSD() {
  if(card_mounted){
    if (server.method() == HTTP_GET) {
      String html = 
        "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
        "<head>"
        "<title>web2rgbmatrix - GIF Upload</title>"
        + style +
        "</head>"
        "<body>"
        "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
        "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
        "<h1>GIF Upload</h1>"
        "<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
        "<label id='file-input' for='file'>   Choose file...</label>"
        "<input id='sub-button' type='submit' class=btn value='Upload'>"
        "<br><br>"
        "<div id='prg'></div>"
        "<br><div id='prgbar'><div id='bar'></div></div><br>"
        "<input id='back-button' type=\"button\" class=btn onclick=\"location.href='/';\" value=\"Back\" />"
        "</form>"
        "<script>"
        "function sub(obj){"
        "var fileName = obj.value.split('\\\\');"
        "document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
        "};"
        "$('form').submit(function(e){"
        "e.preventDefault();"
        "var form = $('#upload_form')[0];"
        "var data = new FormData(form);"
        "$.ajax({"
        "url: '/upload',"
        "type: 'POST',"
        "data: data,"
        "contentType: false,"
        "processData:false,"
        "xhr: function() {"
        "var xhr = new window.XMLHttpRequest();"
        "xhr.upload.addEventListener('progress', function(evt) {"
        "if (evt.lengthComputable) {"
        "var per = evt.loaded / evt.total;"
        "if (evt.loaded != evt.total) {"
        "$('#sub-button').prop('disabled', true);"
        "$('#back-button').prop('disabled', true);"
        "$('#prg').html('Upload Progress: ' + Math.round(per*100) + '%');"
        "} else {"
        "$('#sub-button').prop('disabled', false);"
        "$('#back-button').prop('disabled', false);"
        "var inputFile = $('form').find(\"input[type=file]\");"
        "var fileName = inputFile[0].files[0].name;"
        "$('#prg').html('Upload Success, click GIF to play<br><a href=\"/localplay?file=' + fileName +'\"><img src=\"/gifs/' + fileName + '\"></a>');"
        "}"
        "$('#bar').css('width',Math.round(per*100) + '%');"
        "}"
        "}, false);"
        "return xhr;"
        "},"
        "success:function(d, s) {"
        "console.log('success!') "
        "},"
        "error: function (a, b, c) {"
        "}"
        "});"
        "});"
        "</script>"
        "</body>"
        "</html>";
      returnHTML(html);
    } else {
      returnHTTPError(500, "SD Card Not Mounted");
    }
  } else {
    returnHTTPError(405, "SD Card Not Mounted");
  }
} /* handleSD() */

void handleUpload() {
  if (card_mounted){
    HTTPUpload& uploadfile = server.upload();
    if(uploadfile.status == UPLOAD_FILE_START) {
      String filename = String(uploadfile.filename);
      if(!filename.startsWith("/")) filename = "/gifs/" + String(uploadfile.filename);
      SD.remove(filename);                          // Remove a previous version, otherwise data is appended the file again
      upload_file = SD.open(filename, FILE_WRITE);  // Open the file for writing (create it, if doesn't exist)
      filename = String();
    } else if (uploadfile.status == UPLOAD_FILE_WRITE) {
      if(upload_file) upload_file.write(uploadfile.buf, uploadfile.currentSize); // Write the received bytes to the file
    } else if (uploadfile.status == UPLOAD_FILE_END) {
      if(upload_file) {                                    
        upload_file.close();
        returnHTML("SUCCESS");
      } else {
        returnHTTPError(500, "Couldn't create file");
      }
    }
  } else {
   returnHTTPError(500, "SD Card Not Mounted");
  }
} /* handleUpload() */

void handleRemotePlay(){
  // To upload/play a GIF with curl
  // curl -F 'file=@MENU.gif' http://rgbmatrix.local/remoteplay
  HTTPUpload& uploadfile = server.upload();
  if(uploadfile.status == UPLOAD_FILE_START) {
    LittleFS.remove(gif_filename);                          // Remove a previous version, otherwise data is appended the file again
    upload_file = LittleFS.open(gif_filename, FILE_WRITE);  // Open the file for writing (create it, if doesn't exist)
  } else if (uploadfile.status == UPLOAD_FILE_WRITE) {
    if(upload_file) upload_file.write(uploadfile.buf, uploadfile.currentSize); // Write the received bytes to the file
  } else if (uploadfile.status == UPLOAD_FILE_END) {
    if(upload_file) {                                    
      upload_file.close();
      client_ip = server.client().remoteIP();
      returnHTML("SUCCESS");
      sd_filename = "";
      ShowGIF(gif_filename,false);
    } else {
      returnHTTPError(500, "Couldn't create file");
    }
  }
} /* handleRemotePlay() */

void handleLocalPlay(){
  // To play a GIF from SD card with curl
  // curl http://rgbmatrix.local/localplay?file=MENU.gif
  if (server.method() == HTTP_GET) {
    if (card_mounted){
      if (server.arg("file") != "") {
        String fullpath = "/gifs/" + server.arg("file");
        const char *requested_filename = fullpath.c_str();
        if (!SD.exists(requested_filename)) {
          returnHTTPError(404, "File Not Found");
        } else {
          returnHTML("Displaying local file");
          LittleFS.remove(gif_filename);
          sd_filename = fullpath;
          client_ip = server.client().remoteIP();
          ShowGIF(requested_filename, true);
        } 
      } else {
        returnHTTPError(405, "Method Not Allowed");
      }
    } else {
      returnHTTPError(500, "SD Card Not Mounted");
    }
  } else {
    returnHTTPError(405, "Method Not Allowed");
  }
} /* handleLocalPlay() */

void handleClear(){
  dma_display->clearScreen();
  sd_filename = "";
  config_display_on = false;
  LittleFS.remove(gif_filename);
  String html =
    "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
    "<head>"
    "<title>Display Cleared</title>"
    "<meta http-equiv=\"refresh\" content=\"3\;URL=\'/\'\" />"
    + style +
    "</head>"
    "<body>"
    "<form>"
    "<p>Display Cleared</p>"
    "</form>"
    "</body>"
    "</html>";
  returnHTML(html);
} /* handleClear() */

void handleReboot() {
  String html =
    "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
    "<head>"
      "<title>Rebooting...</title>"
      "<meta http-equiv=\"refresh\" content=\"60\;URL=\'/\'\" />"
    + style +
    "</head>"
    "<body>"
    "<form>"
      "<p>Rebooting...</p>"
    "</form>"
    "</body>"
    "</html>";
  returnHTML(html);
  ESP.restart();
} /* handleReboot() */

void handleNotFound() {
  String path = server.uri();
  String data_type = "text/plain";
  if (path.endsWith("/")) {
    path += "index.html";
  }
  if (path.endsWith(".src")) {
    path = path.substring(0, path.lastIndexOf("."));
  } else if (path.endsWith(".htm")) {
    data_type = "text/html";
  } else if (path.endsWith(".html")) {
    data_type = "text/html";
  } else if (path.endsWith(".css")) {
    data_type = "text/css";
  } else if (path.endsWith(".js")) {
    data_type = "application/javascript";
  } else if (path.endsWith(".png")) {
    data_type = "image/png";
  } else if (path.endsWith(".gif")) {
    data_type = "image/gif";
  } else if (path.endsWith(".jpg")) {
    data_type = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    data_type = "image/x-icon";
  } else if (path.endsWith(".xml")) {
    data_type = "text/xml";
  } else if (path.endsWith(".pdf")) {
    data_type = "application/pdf";
  } else if (path.endsWith(".zip")) {
    data_type = "application/zip";
  }
  
  File data_file;
  if (LittleFS.exists(path.c_str())) {
      data_file = LittleFS.open(path.c_str());
  } else if (card_mounted) {
    if (SD.exists(path.c_str())) {
      data_file = SD.open(path.c_str());
    } else {
      returnHTTPError(404, "File Not Found");
    }
  } 
 
  if (!data_file) {
    returnHTTPError(500, "Couldn't open file");
  }

  if (server.streamFile(data_file, data_type) != data_file.size()) {
    DBG_OUTPUT_PORT.println("Sent less data than expected!");
  }

  data_file.close();
} /* handleNotFound() */

void checkSerialClient() {
  if (DBG_OUTPUT_PORT.available()) {
    new_gif = DBG_OUTPUT_PORT.readStringUntil('\n');
    delay(10);
  }  
  if (new_gif != current_gif) {
    if (card_mounted) {
      LittleFS.remove(gif_filename);
      client_ip = {0,0,0,0};
      if (SD.exists("/gifs/" + new_gif + ".gif")) {
        sd_filename = "/gifs/" + new_gif + ".gif";
        ShowGIF(sd_filename.c_str(), true);
      } else if (SD.exists("/gifs/MENU.gif")){
        sd_filename = "/gifs/MENU.gif";
        ShowGIF(sd_filename.c_str(), true);
      }
    }
  }
  current_gif = new_gif;
} /* checkSerialClient() */

void displaySetup() {
  HUB75_I2S_CFG mxconfig(
    panelResX,           // module width
    panelResY,           // module height
    panels_in_X_chain    // Chain length
  );

  //Pins for a ESP32-Trinity
  mxconfig.gpio.e = 18;
  mxconfig.gpio.b1 = 26;
  mxconfig.gpio.b2 = 12;
  mxconfig.gpio.g1 = 27;
  mxconfig.gpio.g2 = 13;
  mxconfig.clkphase = false;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(brightness); //0-255
  dma_display->clearScreen();
} /* displaySetup() */

// Copy a horizontal span of pixels from a source buffer to an X,Y position
// in matrix back buffer, applying horizontal clipping. Vertical clipping is
// handled in GIFDraw() below -- y can safely be assumed valid here.
void span(uint16_t *src, int16_t x, int16_t y, int16_t width) {
  if (x >= totalWidth) return;     // Span entirely off right of matrix
  int16_t x2 = x + width - 1;      // Rightmost pixel
  if (x2 < 0) return;              // Span entirely off left of matrix
  if (x < 0) {                     // Span partially off left of matrix
    width += x;                    // Decrease span width
    src -= x;                      // Increment source pointer to new start
    x = 0;                         // Leftmost pixel is first column
  }
  if (x2 >= totalWidth) {      // Span partially off right of matrix
    width -= (x2 - totalWidth + 1);
  }
  while(x <= x2) {
    dma_display->drawPixel(x++, y, *src++);
  } 
} /* span() */

// Draw a line of image directly on the matrix
void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y;

  y = pDraw->iY + pDraw->y; // current line

  // Vertical clip
  int16_t screenY = yPos + y; // current row on matrix
  if ((screenY < 0) || (screenY >= totalHeight)) return;

  usPalette = pDraw->pPalette;

  s = pDraw->pPixels;
  
  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) { // if transparency used
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + pDraw->iWidth;
    x = 0;
    iCount = 0;                   // count non-transparent pixels
    while (x < pDraw->iWidth) {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) { // done, stop
          s--;                    // back up to treat it like transparent
        } else {                  // opaque
          *d++ = usPalette[c];
          iCount++;
        }
      }                           // while looking for opaque pixels
      if (iCount) {               // any opaque pixels?
        span(usTemp, xPos + pDraw->iX + x, screenY, iCount);
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent)
          iCount++;
        else
          s--;
      }
      if (iCount) {
        x += iCount; // skip these
        iCount = 0;
      }
    }
  } else {                         //does not have transparency
    s = pDraw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    for (x = 0; x < pDraw->iWidth; x++) 
      usTemp[x] = usPalette[*s++];
    span(usTemp, xPos + pDraw->iX, screenY, pDraw->iWidth);
  }
} /* GIFDraw() */

void * GIFOpenFile(const char *fname, int32_t *pSize) {
  DBG_OUTPUT_PORT.print("Playing gif: ");
  DBG_OUTPUT_PORT.println(fname);
  gif_file = LittleFS.open(fname);
  if (gif_file) {
    *pSize = gif_file.size();
    return (void *)&gif_file;
  }
  return NULL;
} /* GIFOpenFile() */

void * GIFSDOpenFile(const char *fname, int32_t *pSize) {
  DBG_OUTPUT_PORT.print("Playing gif from SD: ");
  DBG_OUTPUT_PORT.println(fname);
  gif_file = SD.open(fname);
  if (gif_file) {
    *pSize = gif_file.size();
    return (void *)&gif_file;
  }
  return NULL;
} /* GIFOpenFile() */

void GIFCloseFile(void *pHandle) {
  File *gif_file = static_cast<File *>(pHandle);
  if (gif_file != NULL)
     gif_file->close();
} /* GIFCloseFile() */

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *gif_file = static_cast<File *>(pFile->fHandle);
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((pFile->iSize - pFile->iPos) < iLen)
       iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0)
       return 0;
    iBytesRead = (int32_t)gif_file->read(pBuf, iBytesRead);
    pFile->iPos = gif_file->position();
    return iBytesRead;
} /* GIFReadFile() */

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) { 
  int i = micros();
  File *gif_file = static_cast<File *>(pFile->fHandle);
  gif_file->seek(iPosition);
  pFile->iPos = (int32_t)gif_file->position();
  i = micros() - i;
  //DBG_OUTPUT_PORT.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */

void ShowGIF(const char *name, bool sd) {
  config_display_on = false;
  dma_display->clearScreen();
  if (sd && card_mounted){
    if (gif.open(name, GIFSDOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
      DBG_OUTPUT_PORT.printf("Successfully opened GIF from SD; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
      DBG_OUTPUT_PORT.flush();
      while (gif.playFrame(true, NULL)) {}
      gif.close();
    }    
  } else {
    if (gif.open(name, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
      DBG_OUTPUT_PORT.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
      DBG_OUTPUT_PORT.flush();
      while (gif.playFrame(true, NULL)) {}
      gif.close();
    }
  }
} /* ShowGIF() */