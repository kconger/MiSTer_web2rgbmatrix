/*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
* You can download the latest version of this code from:
* https://github.com/kconger/MiSTer_web2rgbmatrix
*/
#include "FS.h"
#include "SPI.h"
#include <AnimatedGIF.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32FtpServer.h>
#include <ESP32Ping.h>
#include <ESPmDNS.h>
#include <ezTime.h>
#include <FastLED.h>
#include <LittleFS.h>
#include <SdFat.h>
#include <TetrisMatrixDraw.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include "bitmaps.h"


#define VERSION "20230122"

#define DEFAULT_TIMEZONE "Europe/Warsaw" // https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
char timezone[80] = DEFAULT_TIMEZONE;

#define DEFAULT_TWELVEHOUR false
bool twelvehour = DEFAULT_TWELVEHOUR;

#define DEFAULT_SSID "Madsdsadsadsads"
char ssid[80] = DEFAULT_SSID;

#define DEFAULT_PASSWORD "password"
char password[80] = DEFAULT_PASSWORD;

#define DEFAULT_AP_SSID "rgbmatrix"
char ap[80] = DEFAULT_AP_SSID;

#define DEFAULT_AP_PASSWORD "password"
char ap_password[80] = DEFAULT_PASSWORD;

#define DEFAULT_HOSTNAME "rgbmatrix"
char hostname[80] = DEFAULT_HOSTNAME;

#define DEFAULT_TEXT_COLOR "#FFFFFF" // White
String textcolor = DEFAULT_TEXT_COLOR;

#define DEFAULT_BRIGHTNESS 255
uint8_t matrix_brightness = DEFAULT_BRIGHTNESS;

#define DEFAULT_GIF_PLAYBACK "Both" // Animated | Static | Both | Fallback
String playback = DEFAULT_GIF_PLAYBACK;

#define DEFAULT_SCREENSAVER "Blank" // Blank | Clock | Plasma | Starfield | Toasters
String screensaver = DEFAULT_SCREENSAVER;

#define DEFAULT_SCREENSAVER_COLOR "#FFFFFF" // White
String accentcolor = DEFAULT_SCREENSAVER_COLOR;

#define DEFAULT_PING_FAIL_COUNT 2 // 30s increments, set to '0' to disable client ping check
int ping_fail_count = DEFAULT_PING_FAIL_COUNT;

#define DEFAULT_SD_STATIC_GIF_FOLDER "/static/"
char gif_folder[80] = DEFAULT_SD_STATIC_GIF_FOLDER;

#define DEFAULT_SD_ANIMATED_GIF_FOLDER "/animated/"
char animated_gif_folder[80] = DEFAULT_SD_ANIMATED_GIF_FOLDER;

#define DBG_OUTPUT_PORT Serial

// Pins for a ESP32-Trinity
// Matrix pins
#define E 18
#define B1 26
#define B2 12
#define G1 27
#define G2 13
#define R1 25
#define R2 14
// SD Card reader pins
#define SD_SCLK 33
#define SD_MISO 32
#define SD_MOSI 21
#define SD_SS 22
// Matrix Alt Config Pin - Connect to ground to swap Blue/Green
#define ALT 2

SPIClass spi = SPIClass(HSPI);

// Matrix Config
// See the "displaySetup" method for more display config options
MatrixPanel_I2S_DMA *matrix_display = nullptr;
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
FtpServer ftp_server;

// Style for HTML pages
String style =
  "<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
  "a{outline:none;text-decoration:none;padding: 2px 1px 0;color:#777;}a:link{color:#777;}a:hover{solid;color:#3498db;}"
  "input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
  "#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
  "#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
  "form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
  ".btn{background:#3498db;color:#fff;cursor:pointer}.btn:disabled,.btn.disabled{background:#ddd;color:#fff;cursor:not-allowed;pointer-events: none}"
  ".actionbtn{background:#218838;color:#fff;cursor:pointer}.actionbtn:disabled,.actionbtn.disabled{background:#ddd;color:#fff;cursor:not-allowed;pointer-events: none}"
  ".cautionbtn{background:#c82333;color:#fff;cursor:pointer}.cautionbtn:disabled,.cautionbtn.disabled{background:#ddd;color:#fff;cursor:not-allowed;pointer-events: none}"
  "input[type=\"checkbox\"]{margin:0px;width:22px;height:22px;}"
  "table{width: 100%;}select{width: 100%;height:44px;}"
  "</style>";

String homepage = "https://github.com/kconger/MiSTer_web2rgbmatrix";
String latest_url = "https://raw.githubusercontent.com/kconger/MiSTer_web2rgbmatrix/master/releases/LATEST";
String releases = homepage + "/tree/master/releases";
const char *config_filename = "/secrets.json";
const char *gif_filename = "/temp.gif";

String wifi_mode = "AP";
String sd_status = "";
bool card_mounted = false;
bool config_display_on = true;
bool tty_client = false;
unsigned long last_seen, start_tick;
int ping_fail = 0;
String sd_filename = "";
File gif_file, upload_file;
String new_command = "";
String current_command = "";
AnimatedGIF gif;

// Plasma Screen Saver
uint16_t time_counter = 0, cycles = 0;
CRGB currentColor;
CRGBPalette16 palettes[] = {HeatColors_p, LavaColors_p, RainbowColors_p, RainbowStripeColors_p, CloudColors_p};
CRGBPalette16 currentPalette = palettes[0];
CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}

// Starfield Screen Saver
const int starCount = 256; // number of stars in the star field
const int maxDepth = 32;   // maximum distance away for a star
// the star field - starCount stars represented as x, y and z co-ordinates
double stars[starCount][3];
CRGB *matrix_buffer;

// Clock Screen Saver
// If this is set to false, the number will only change if the value behind it changes
// e.g. the digit representing the least significant minute will be replaced every minute,
// but the most significant number will only be replaced every 10 minutes.
// When true, all digits will be replaced every minute.
bool forceRefresh = true;
unsigned long animationDue = 0;
unsigned long animationDelay = 100; // Smaller number == faster
TetrisMatrixDraw tetris(*matrix_display); // Main clock
TetrisMatrixDraw tetris2(*matrix_display); // The "M" of AM/PM
TetrisMatrixDraw tetris3(*matrix_display); // The "P" or "A" of AM/PM
Timezone myTZ;
unsigned long oneSecondLoopDue = 0;
bool showColon = true;
volatile bool finishedAnimating = false;
String lastDisplayedTime = "";
String lastDisplayedAmPm = "";
const int tetrisYOffset = (totalHeight) / 2;
const int tetrisXOffset = (panelResX) / 2;

// Toaster Screen Saver
#define N_FLYERS   5 // Number of flying things
struct Flyer {       // Array of flying things
  int16_t x, y;      // Top-left position * 16 (for subpixel pos updates)
  int8_t  depth;     // Stacking order is also speed, 12-24 subpixels/frame
  uint8_t frame;     // Animation frame; Toasters cycle 0-3, Toast=255
} flyer[N_FLYERS];

void setup(void) {    
  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.setDebugOutput(true);
  delay(1000);

  pinMode(ALT, INPUT_PULLUP);

  // Initialize internal filesystem
  DBG_OUTPUT_PORT.println("Loading LITTLEFS");
  if(!LittleFS.begin(true)){
    DBG_OUTPUT_PORT.println("LITTLEFS Mount Failed");
  }
  LittleFS.remove(gif_filename);

  //Read Config
  parseConfig();

  // Initialize Wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  DBG_OUTPUT_PORT.print("Connecting to ");DBG_OUTPUT_PORT.println(ssid);
    
  // Wait for connection
  uint8_t i = 0;
  while ((WiFi.status() != WL_CONNECTED) && (i++ < 20)) { //wait 10 seconds
    delay(500);
  }
  if (i == 21) {
    DBG_OUTPUT_PORT.print("Could not connect to ");DBG_OUTPUT_PORT.println(ssid);
    // Startup Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap, ap_password);
    my_ip = WiFi.softAPIP();
    DBG_OUTPUT_PORT.print("IP address: ");DBG_OUTPUT_PORT.println(my_ip.toString());
  } else {
    my_ip = WiFi.localIP();
    wifi_mode = "Infrastructure";
    DBG_OUTPUT_PORT.print("Connected! IP address: ");DBG_OUTPUT_PORT.println(my_ip.toString());
  }
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  // Setup EZ Time
  if (WiFi.status() == WL_CONNECTED ) {
    setDebug(INFO);
    waitForSync();
    DBG_OUTPUT_PORT.println("UTC: " + UTC.dateTime());
    myTZ.setLocation(F(timezone));
    DBG_OUTPUT_PORT.print(F("Time in your timezone: "));
    DBG_OUTPUT_PORT.println(myTZ.dateTime());
  }

  // Initialize SD Card
  spi.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_SS);
  if (SD.begin(SD_SS, spi, 8000000)) {
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
      ftp_server.begin(SD, ap, ap_password); 
    }
  } else {
    DBG_OUTPUT_PORT.println("Card Mount Failed");
    sd_status = "Mount Failed";
  }

  // Startup MDNS 
  if (MDNS.begin(hostname)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ftp", "tcp", 21);
    DBG_OUTPUT_PORT.println("MDNS responder started");
    DBG_OUTPUT_PORT.print("You can now connect to ");
    DBG_OUTPUT_PORT.print(hostname);
    DBG_OUTPUT_PORT.println(".local");
  }

  // Startup Webserver
  server.on("/", handleRoot);
  server.on("/settings", handleSettings);
  server.on("/ota", handleOTA);
  server.on("/update", HTTP_POST, [](){
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "OTA Update Failure\n" : "OTA Update Success\n");
    delay(2000);
    matrix_display->clearScreen();
    ESP.restart();
  }, handleUpdate);
  server.on("/sdcard", handleSD);
  server.on("/upload", HTTP_POST, [](){server.sendHeader("Connection", "close");}, handleUpload);
  server.on("/list", HTTP_GET, handleFileList);
  server.on("/delete", HTTP_GET, handleFileDelete);
  server.on("/sdplay", handleSDPlay);
  server.on("/localplay", handleLocalPlay);
  server.on("/remoteplay", HTTP_POST, [](){server.send(200);}, handleRemotePlay);
  server.on("/text", handleText);
  server.on("/version", handleVersion);
  server.on("/clear", handleClear);
  server.on("/reboot", handleReboot);
  server.onNotFound(handleNotFound);
  const char *headerkeys[] = {"Content-Length"};
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);
  server.collectHeaders(headerkeys, headerkeyssize);
  server.begin();

  // Initialize gif object
  gif.begin(LITTLE_ENDIAN_PIXELS);

  // Initialize Display
  displaySetup();

  // Initialise the star field with random stars
  for (int i = 0; i < starCount; i++) {
    stars[i][0] = getRandom(-25, 25);
    stars[i][1] = getRandom(-25, 25);
    stars[i][2] = getRandom(0, maxDepth);
  }
  matrix_buffer = (CRGB *)malloc(((totalWidth) * (totalHeight)) * sizeof(CRGB));
  bufferClear(matrix_buffer);

  // Initialize Tetris clock
  tetris.display = matrix_display; // Main clock
  tetris2.display = matrix_display; // The "M" of AM/PM
  tetris3.display = matrix_display; // The "P" or "A" of AM/PM
  finishedAnimating = false;
  tetris.scale = 2;

  //Initialize Toaster Screen Saver
  randomSeed(analogRead(2));
  for(uint8_t i=0; i<N_FLYERS; i++) {  // Randomize initial flyer states
    flyer[i].x     = (-32 + random(160)) * 16;
    flyer[i].y     = (-32 + random( 96)) * 16;
    flyer[i].frame = random(3) ? random(4) : 255; // 66% toaster, else toast
    flyer[i].depth = 10 + random(16);             // Speed / stacking order
  }
  qsort(flyer, N_FLYERS, sizeof(struct Flyer), compare); // Sort depths

  // Display boot status on matrix
  showStatus();

  start_tick = millis();

  DBG_OUTPUT_PORT.println("Startup Complete");
} /* setup() */

void loop(void) {
  // Clear initial boot display after 1min
  if (config_display_on && (millis() - start_tick >= 60*1000UL)){
    config_display_on = false;
    matrix_display->clearScreen();
    matrix_display->setBrightness8(matrix_brightness);
  }
  server.handleClient();  // Handle Web Client
  ftp_server.handleFTP(); // Handle FTP Client
  checkSerialClient();    // tty2x Client Check
  checkClientTimeout();   // Client Timeout Check
} /* loop() */

bool parseConfig() {
  // Open file for parsing
  File config_file = LittleFS.open(config_filename);
  if (!config_file) {
    DBG_OUTPUT_PORT.println("ERROR: Could not open secrets.json file for reading!");
    return false;
  }

  // Check if we can deserialize the secrets.json file
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, config_file);
  if (err) {
    DBG_OUTPUT_PORT.println("ERROR: deserializeJson() failed with code ");
    DBG_OUTPUT_PORT.println(err.c_str());
    return false;
  }

  // Read settings from secrets.json file
  strlcpy(ssid, doc["ssid"] | DEFAULT_SSID, sizeof(ssid));
  strlcpy(password, doc["password"] | DEFAULT_PASSWORD, sizeof(password));
  ping_fail_count = doc["timeout"] | DEFAULT_PING_FAIL_COUNT;
  matrix_brightness = doc["brightness"] | DEFAULT_BRIGHTNESS;
  textcolor = doc["textcolor"] | DEFAULT_TEXT_COLOR;
  playback = doc["playback"] | DEFAULT_GIF_PLAYBACK;
  screensaver = doc["screensaver"] | DEFAULT_SCREENSAVER;
  accentcolor = doc["accentcolor"] | DEFAULT_SCREENSAVER_COLOR;
  twelvehour = doc["twelvehour"] | DEFAULT_TWELVEHOUR;
  strlcpy(timezone, doc["timezone"] | DEFAULT_TIMEZONE, sizeof(timezone));
       
  // Close the secrets.json file
  config_file.close();
  return true;
} /* parseConfig() */

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  DBG_OUTPUT_PORT.println("Connected to AP successfully!");
} /* WiFiStationConnected() */

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  DBG_OUTPUT_PORT.println("WiFi connected");
  DBG_OUTPUT_PORT.println("IP address: ");
  DBG_OUTPUT_PORT.println(WiFi.localIP());
  my_ip = WiFi.localIP();
} /* WiFiGotIP() */

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  DBG_OUTPUT_PORT.print("WiFi lost connection. Reason: ");
  DBG_OUTPUT_PORT.println(info.wifi_sta_disconnected.reason);
  DBG_OUTPUT_PORT.println("Trying to Reconnect");
  WiFi.begin(ssid, password);
} /* WiFiStationDisconnected() */

void returnHTML(String html) {
  server.send(200, F("text/html"), html + "\r\n");
} /* returnHTML() */

void returnHTTPError(int code, String msg) {
  server.send(code, F("text/plain"), msg + "\r\n");
} /* returnHTTPError() */

void handleRoot() {
  if (server.method() == HTTP_GET) {
    String image_status = "";
    if (LittleFS.exists(gif_filename)){
      image_status = "<tr>"
       "<td>Client</td>"
       "<td>" + client_ip.toString() + "</td>"
       "</tr>"
       "<tr>"
       "<td>Current Image</td>"
       "<td><img src=\"" + String(gif_filename) + "\"><img></td>"
       "</tr>";
    } else if (sd_filename != ""){
      image_status = "<tr>"
       "<td>Client</td>"
       "<td>" + ((tty_client) ? "Serial" : client_ip.toString()) + "</td>"
       "</tr>"
       "<tr>"
       "<td>Current Image</td>"
       "<td><img src=\"" + String(sd_filename) + "\"><img></td>"
       "</tr>";
    }
    String html =
      "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
      "<head>"
      "<title>web2rgbmatrix</title>"
      + style +
      "</head>"
      "<body>"
      "<form action=\"/\">"
      "<a href=\""+ homepage + "\"><h1>web2rgbmatrix</h1></a><br>"
      "<p>"
      "<h3>Status</h3>"
      "<table>"
      "<tr><td>Version</td><td>" + VERSION + "</td></tr>"
      "<tr><td>SD Card</td><td>" + sd_status + "</td></tr>"
      "<tr><td>Wifi Mode</td><td>" + wifi_mode + "</td></tr>"
      "<tr><td>rgbmatrix IP</td><td>" + my_ip.toString() + "</td></tr>"
      + image_status + 
      "</table>"
      "</p>"
      "<input type=\"button\" class=actionbtn onclick=\"location.href='/clear';\" value=\"Clear Display\" />"
      + ((card_mounted) ? "<input type=\"button\" class=btn onclick=\"location.href='/sdcard';\" value=\"GIF Upload\" /><input type=\"button\" class=btn onclick=\"location.href='/list?dir=/';\" value=\"File Browser\" />" : "") +
      "<input type=\"button\" class=btn onclick=\"location.href='/settings';\" value=\"Settings\" />"
      "<input type=\"button\" class=btn onclick=\"location.href='/ota';\" value=\"OTA Update\" />"
      "<input type=\"button\" class=cautionbtn onclick=\"location.href='/reboot';\" value=\"Reboot\" />"
      "</form>"
      "</body>"
      "</html>";
    returnHTML(html);
  } else {
    returnHTTPError(405, "Method Not Allowed");
  }
} /* handleRoot() */

void handleSettings() {
  String playback_select_items =
    "<option value=\"Animated\"" + String((playback == "Animated") ? " selected" : "") + ">Animated</option>"
    "<option value=\"Static\"" + String((playback == "Static") ? " selected" : "") + ">Static</option>"
    "<option value=\"Both\"" + String((playback == "Both") ? " selected" : "") + ">Animated then Static</option>"
    "<option value=\"Fallback\"" + String((playback == "Fallback") ? " selected" : "") + ">Static Fallback</option>";

  String saver_select_items =
    "<option value=\"Blank\"" + String((screensaver == "Blank") ? " selected" : "") + ">Blank</option>"
    "<option value=\"Clock\"" + String((screensaver == "Clock") ? " selected" : "") + ">Clock</option>"
    "<option value=\"Plasma\"" + String((screensaver == "Plasma") ? " selected" : "") + ">Plasma</option>"
    "<option value=\"Starfield\"" + String((screensaver == "Starfield") ? " selected" : "") + ">Starfield</option>"
    "<option value=\"Toasters\"" + String((screensaver == "Toasters") ? " selected" : "") + ">Toasters</option>";

  String tz_select_items =
    "<option value=\"Etc/GMT+12\"" + String((String(timezone) == "Etc/GMT+12") ? " selected" : "") + ">(GMT-12:00) International Date Line West</option>"
    "<option value=\"Pacific/Midway\"" + String((String(timezone) == "Pacific/Midway") ? " selected" : "") + ">(GMT-11:00) Midway Island, Samoa</option>"
    "<option value=\"Pacific/Honolulu\"" + String((String(timezone) == "Pacific/Honolulu") ? " selected" : "") + ">(GMT-10:00) Hawaii</option>"
    "<option value=\"America/Anchorage\"" + String((String(timezone) == "America/Anchorage") ? " selected" : "") + ">(GMT-09:00) Alaska</option>"
    "<option value=\"America/Los_Angeles\"" + String((String(timezone) == "America/Los_Angeles") ? " selected" : "") + ">(GMT-08:00) Pacific Time (US & Canada)</option>"
    "<option value=\"America/Tijuana\"" + String((String(timezone) == "America/Tijuana") ? " selected" : "") + ">(GMT-08:00) Tijuana, Baja California</option>"
    "<option value=\"America/Phoenix\"" + String((String(timezone) == "America/Phoenix") ? " selected" : "") + ">(GMT-07:00) Arizona</option>"
    "<option value=\"America/Chihuahua\"" + String((String(timezone) == "America/Chihuahua") ? " selected" : "") + ">(GMT-07:00) Chihuahua, La Paz, Mazatlan</option>"
    "<option value=\"America/Denver\"" + String((String(timezone) == "America/Denver") ? " selected" : "") + ">(GMT-07:00) Mountain Time (US & Canada)</option>"
    "<option value=\"America/Managua\"" + String((String(timezone) == "America/Managua") ? " selected" : "") + ">(GMT-06:00) Central America</option>"
    "<option value=\"America/Chicago\"" + String((String(timezone) == "America/Chicago") ? " selected" : "") + ">(GMT-06:00) Central Time (US & Canada)</option>"
    "<option value=\"America/Mexico_City\"" + String((String(timezone) == "America/Mexico_City") ? " selected" : "") + ">(GMT-06:00) Guadalajara, Mexico City, Monterrey</option>"
    "<option value=\"Canada/Saskatchewan\"" + String((String(timezone) == "Canada/Saskatchewan") ? " selected" : "") + ">(GMT-06:00) Saskatchewan</option>"
    "<option value=\"America/Bogota\"" + String((String(timezone) == "America/Bogota") ? " selected" : "") + ">(GMT-05:00) Bogota, Lima, Quito, Rio Branco</option>"
    "<option value=\"America/New_York\"" + String((String(timezone) == "America/New_York") ? " selected" : "") + ">(GMT-05:00) Eastern Time (US & Canada)</option>"
    "<option value=\"America/Indiana/Knox\"" + String((String(timezone) == "America/Indiana/Knox") ? " selected" : "") + ">(GMT-05:00) Indiana (East)</option>"
    "<option value=\"Canada/Atlantic\"" + String((String(timezone) == "Canada/Atlantic") ? " selected" : "") + ">(GMT-04:00) Atlantic Time (Canada)</option>"
    "<option value=\"America/Caracas\"" + String((String(timezone) == "America/Caracas") ? " selected" : "") + ">(GMT-04:00) Caracas, La Paz</option>"
    "<option value=\"America/Manaus\"" + String((String(timezone) == "America/Manaus") ? " selected" : "") + ">(GMT-04:00) Manaus</option>"
    "<option value=\"America/Santiago\"" + String((String(timezone) == "America/Santiago") ? " selected" : "") + ">(GMT-04:00) Santiago</option>"
    "<option value=\"Canada/Newfoundland\"" + String((String(timezone) == "Canada/Newfoundland") ? " selected" : "") + ">(GMT-03:30) Newfoundland</option>"
    "<option value=\"America/Sao_Paulo\"" + String((String(timezone) == "America/Sao_Paulo") ? " selected" : "") + ">(GMT-03:00) Brasilia</option>"
    "<option value=\"America/Argentina/Buenos_Aires\"" + String((String(timezone) == "America/Argentina/Buenos_Aires") ? " selected" : "") + ">(GMT-03:00) Buenos Aires, Georgetown</option>"
    "<option value=\"America/Godthab\"" + String((String(timezone) == "America/Godthab") ? " selected" : "") + ">(GMT-03:00) Greenland</option>"
    "<option value=\"America/Montevideo\"" + String((String(timezone) == "America/Montevideo") ? " selected" : "") + ">(GMT-03:00) Montevideo</option>"
    "<option value=\"America/Noronha\"" + String((String(timezone) == "America/Noronha") ? " selected" : "") + ">(GMT-02:00) Mid-Atlantic</option>"
    "<option value=\"Atlantic/Cape_Verde\"" + String((String(timezone) == "Atlantic/Cape_Verde") ? " selected" : "") + ">(GMT-01:00) Cape Verde Is.</option>"
    "<option value=\"Atlantic/Azores\"" + String((String(timezone) == "Atlantic/Azores") ? " selected" : "") + ">(GMT-01:00) Azores</option>"
    "<option value=\"Africa/Casablanca\"" + String((String(timezone) == "Africa/Casablanca") ? " selected" : "") + ">(GMT+00:00) Casablanca, Monrovia, Reykjavik</option>"
    "<option value=\"Etc/Greenwich\"" + String((String(timezone) == "Etc/Greenwich") ? " selected" : "") + ">(GMT+00:00) Greenwich Mean Time : Dublin, Edinburgh, Lisbon, London</option>"
    "<option value=\"Europe/Amsterdam\"" + String((String(timezone) == "Europe/Amsterdam") ? " selected" : "") + ">(GMT+01:00) Amsterdam, Berlin, Bern, Rome, Stockholm, Vienna</option>"
    "<option value=\"Europe/Belgrade\"" + String((String(timezone) == "Europe/Belgrade") ? " selected" : "") + ">(GMT+01:00) Belgrade, Bratislava, Budapest, Ljubljana, Prague</option>"
    "<option value=\"Europe/Brussels\"" + String((String(timezone) == "Europe/Brussels") ? " selected" : "") + ">(GMT+01:00) Brussels, Copenhagen, Madrid, Paris</option>"
    "<option value=\"Europe/Sarajevo\"" + String((String(timezone) == "Europe/Sarajevo") ? " selected" : "") + ">(GMT+01:00) Sarajevo, Skopje, Warsaw, Zagreb</option>"
    "<option value=\"Africa/Lagos\"" + String((String(timezone) == "Africa/Lagos") ? " selected" : "") + ">(GMT+01:00) West Central Africa</option>"
    "<option value=\"Asia/Amman\"" + String((String(timezone) == "Asia/Amman") ? " selected" : "") + ">(GMT+02:00) Amman</option>"
    "<option value=\"Europe/Athens\"" + String((String(timezone) == "Europe/Athens") ? " selected" : "") + ">(GMT+02:00) Athens, Bucharest, Istanbul</option>"
    "<option value=\"Asia/Beirut\"" + String((String(timezone) == "Asia/Beirut") ? " selected" : "") + ">(GMT+02:00) Beirut</option>"
    "<option value=\"Africa/Cairo\"" + String((String(timezone) == "Africa/Cairo") ? " selected" : "") + ">(GMT+02:00) Cairo</option>"
    "<option value=\"Africa/Harare\"" + String((String(timezone) == "Africa/Harare") ? " selected" : "") + ">(GMT+02:00) Harare, Pretoria</option>"
    "<option value=\"Europe/Helsinki\"" + String((String(timezone) == "Europe/Helsinki") ? " selected" : "") + ">(GMT+02:00) Helsinki, Kyiv, Riga, Sofia, Tallinn, Vilnius</option>"
    "<option value=\"Asia/Jerusalem\"" + String((String(timezone) == "Asia/Jerusalem") ? " selected" : "") + ">(GMT+02:00) Jerusalem</option>"
    "<option value=\"Europe/Minsk\"" + String((String(timezone) == "Europe/Minsk") ? " selected" : "") + ">(GMT+02:00) Minsk</option>"
    "<option value=\"Africa/Windhoek\"" + String((String(timezone) == "Africa/Windhoek") ? " selected" : "") + ">(GMT+02:00) Windhoek</option>"
    "<option value=\"Asia/Kuwait\"" + String((String(timezone) == "Asia/Kuwait") ? " selected" : "") + ">(GMT+03:00) Kuwait, Riyadh, Baghdad</option>"
    "<option value=\"Europe/Moscow\"" + String((String(timezone) == "Europe/Moscow") ? " selected" : "") + ">(GMT+03:00) Moscow, St. Petersburg, Volgograd</option>"
    "<option value=\"Africa/Nairobi\"" + String((String(timezone) == "Africa/Nairobi") ? " selected" : "") + ">(GMT+03:00) Nairobi</option>"
    "<option value=\"Asia/Tbilisi\"" + String((String(timezone) == "Asia/Tbilisi") ? " selected" : "") + ">(GMT+03:00) Tbilisi</option>"
    "<option value=\"Asia/Tehran\"" + String((String(timezone) == "Asia/Tehran") ? " selected" : "") + ">(GMT+03:30) Tehran</option>"
    "<option value=\"Asia/Muscat\"" + String((String(timezone) == "Asia/Muscat") ? " selected" : "") + ">(GMT+04:00) Abu Dhabi, Muscat</option>"
    "<option value=\"Asia/Baku\"" + String((String(timezone) == "Asia/Baku") ? " selected" : "") + ">(GMT+04:00) Baku</option>"
    "<option value=\"Asia/Yerevan\"" + String((String(timezone) == "Asia/Yerevan") ? " selected" : "") + ">(GMT+04:00) Yerevan</option>"
    "<option value=\"Asia/Kabul\"" + String((String(timezone) == "Asia/Kabul") ? " selected" : "") + ">(GMT+04:30) Kabul</option>"
    "<option value=\"Asia/Yekaterinburg\"" + String((String(timezone) == "Asia/Yekaterinburg") ? " selected" : "") + ">(GMT+05:00) Yekaterinburg</option>"
    "<option value=\"Asia/Karachi\"" + String((String(timezone) == "Asia/Karachi") ? " selected" : "") + ">(GMT+05:00) Islamabad, Karachi, Tashkent</option>"
    "<option value=\"Asia/Calcutta\"" + String((String(timezone) == "Asia/Calcutta") ? " selected" : "") + ">(GMT+05:30) Chennai, Kolkata, Mumbai, New Delhi</option>"
    "<option value=\"Asia/Katmandu\"" + String((String(timezone) == "Asia/Katmandu") ? " selected" : "") + ">(GMT+05:45) Kathmandu</option>"
    "<option value=\"Asia/Almaty\"" + String((String(timezone) == "Asia/Almaty") ? " selected" : "") + ">(GMT+06:00) Almaty, Novosibirsk</option>"
    "<option value=\"Asia/Dhaka\"" + String((String(timezone) == "Asia/Dhaka") ? " selected" : "") + ">(GMT+06:00) Astana, Dhaka</option>"
    "<option value=\"Asia/Rangoon\"" + String((String(timezone) == "Asia/Rangoon") ? " selected" : "") + ">(GMT+06:30) Yangon (Rangoon)</option>"
    "<option value=\"Asia/Bangkok\"" + String((String(timezone) == "Asia/Bangkok") ? " selected" : "") + ">(GMT+07:00) Bangkok, Hanoi, Jakarta</option>"
    "<option value=\"Asia/Krasnoyarsk\"" + String((String(timezone) == "Asia/Krasnoyarsk") ? " selected" : "") + ">(GMT+07:00) Krasnoyarsk</option>"
    "<option value=\"Asia/Hong_Kong\"" + String((String(timezone) == "Asia/Hong_Kong") ? " selected" : "") + ">(GMT+08:00) Beijing, Chongqing, Hong Kong, Urumqi</option>"
    "<option value=\"Asia/Kuala_Lumpur\"" + String((String(timezone) == "Asia/Kuala_Lumpur") ? " selected" : "") + ">(GMT+08:00) Kuala Lumpur, Singapore</option>"
    "<option value=\"Asia/Irkutsk\"" + String((String(timezone) == "Asia/Irkutsk") ? " selected" : "") + ">(GMT+08:00) Irkutsk, Ulaan Bataar</option>"
    "<option value=\"Australia/Perth\"" + String((String(timezone) == "Australia/Perth") ? " selected" : "") + ">(GMT+08:00) Perth</option>"
    "<option value=\"Asia/Taipei\"" + String((String(timezone) == "Asia/Taipei") ? " selected" : "") + ">(GMT+08:00) Taipei</option>"
    "<option value=\"Asia/Tokyo\"" + String((String(timezone) == "Asia/Tokyo") ? " selected" : "") + ">(GMT+09:00) Osaka, Sapporo, Tokyo</option>"
    "<option value=\"Asia/Seoul\"" + String((String(timezone) == "Asia/Seoul") ? " selected" : "") + ">(GMT+09:00) Seoul</option>"
    "<option value=\"Asia/Yakutsk\"" + String((String(timezone) == "Asia/Yakutsk") ? " selected" : "") + ">(GMT+09:00) Yakutsk</option>"
    "<option value=\"Australia/Adelaide\"" + String((String(timezone) == "Australia/Adelaide") ? " selected" : "") + ">(GMT+09:30) Adelaide</option>"
    "<option value=\"Australia/Darwin\"" + String((String(timezone) == "Australia/Darwin") ? " selected" : "") + ">(GMT+09:30) Darwin</option>"
    "<option value=\"Australia/Brisbane\"" + String((String(timezone) == "Australia/Brisbane") ? " selected" : "") + ">(GMT+10:00) Brisbane</option>"
    "<option value=\"Australia/Canberra\"" + String((String(timezone) == "Australia/Canberra") ? " selected" : "") + ">(GMT+10:00) Canberra, Melbourne, Sydney</option>"
    "<option value=\"Australia/Hobart\"" + String((String(timezone) == "Australia/Hobart") ? " selected" : "") + ">(GMT+10:00) Hobart</option>"
    "<option value=\"Pacific/Guam\"" + String((String(timezone) == "Pacific/Guam") ? " selected" : "") + ">(GMT+10:00) Guam, Port Moresby</option>"
    "<option value=\"Asia/Vladivostok\"" + String((String(timezone) == "Asia/Vladivostok") ? " selected" : "") + ">(GMT+10:00) Vladivostok</option>"
    "<option value=\"Asia/Magadan\"" + String((String(timezone) == "Asia/Magadan") ? " selected" : "") + ">(GMT+11:00) Magadan, Solomon Is., New Caledonia</option>"
    "<option value=\"Pacific/Auckland\"" + String((String(timezone) == "Pacific/Auckland") ? " selected" : "") + ">(GMT+12:00) Auckland, Wellington</option>"
    "<option value=\"Pacific/Fiji\"" + String((String(timezone) == "Pacific/Fiji") ? " selected" : "") + ">(GMT+12:00) Fiji, Kamchatka, Marshall Is.</option>"
    "<option value=\"Pacific/Tongatapu\"" + String((String(timezone) == "Pacific/Tongatapu") ? " selected" : "") + ">(GMT+13:00) Nuku'alofa</option>";
  
  String html =
    "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
    "<head>"
    "<title>web2rgbmatrix - Settings</title>"
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
    "<form action=\"/settings\">"
    "<h1>Settings</h1>"
    "<p>"
    "<h3>Wifi Client Settings</h3>"
    "SSID<br>"
    "<input type=\"text\" name=\"ssid\" value=\"" + String(ssid) + "\"><br>"
    "Password<br>"
    "<input type=\"password\" name=\"password\" id=\"idPassword\" value=\"" + String(password) + "\"><br>"
    "<div>"
    "<label for=\"showpass\" class=\"chkboxlabel\">"
    "<input type=\"checkbox\"id=\"showpass\" onclick=\"myFunction()\">"
    " Show Password</label>"
    "</div>"
    "</p>"
    "<p>"
    "<h3>Display Settings</h3>"
    "<label for=\"textcolor\">Text Color</label>"
    "<input type=\"color\" id=\"textcolor\" name=\"textcolor\" value=\"" + textcolor + "\">"
    "<label for=\"brightness\">LED Brightness</label>"
    "<input type=\"number\" id=\"brightness\" name=\"brightness\" min=\"0\" max=\"255\" value=" + matrix_brightness + ">"
    "<label for=\"playback\">GIF Playback</label><br>"
    "<br>"
    "<select id=\"playback\" name=\"playback\" value=\"" + playback + "\">"
    + playback_select_items +
    "</select>"
    "<h3>Screen Saver Settings</h3>"
    "<label for=\"screensaver\">Screen Saver</label><br>"
    "<br>"
    "<select id=\"screensaver\" name=\"screensaver\" value=\"" + screensaver + "\">"
    + saver_select_items +
    "</select>"
    "<br><br>"
    "<label for=\"accentcolor\">Accent Color</label>"
    "<input type=\"color\" id=\"accentcolor\" name=\"accentcolor\" value=\"" + accentcolor + "\">"
    "<label for=\"timeout\">Client Timeout(Minutes)</label>"
    "<input type=\"number\" id=\"timeout\" name=\"timeout\" min=\"0\" max=\"60\" value=" + (ping_fail_count / 2) + ">"
    "<label for=\"timezone\">Timezone</label><br>"
    "<br>"
    "<select id=\"timezone\" name=\"timezone\" value=\"" + String(timezone) + "\">"
    + tz_select_items +
    "</select>"
    "<br><br>"
    "<label for=\"twelvehour\" class=\"chkboxlabel\">"
    "<input type=\"checkbox\" id=\"twelvehour\" name=\"twelvehour\" value=\"true\"" + (twelvehour ? " checked=\"checked\"" : "") + "/>"
    " 12hr Format</label>"
    "</p>"
    "<input type=\"submit\" class=actionbtn value=\"Save\"><br>"
    "<input id='back-button' type=\"button\" class=btn onclick=\"location.href='/';\" value=\"Back\" />"
    "</form>"
    "</body>"
    "</html>";
  if (server.method() == HTTP_GET) {
    if (server.arg("ssid") != "" && server.arg("password") != "" && server.arg("brightness") != "" && server.arg("timeout") != "" 
      && server.arg("textcolor") != "" && server.arg("screensaver") != "" && server.arg("timezone") != ""){

      server.arg("ssid").toCharArray(ssid, sizeof(ssid));
      server.arg("password").toCharArray(password, sizeof(password));
      if (server.arg("timeout") == "0"){
        ping_fail_count = 0;
      } else {
        ping_fail_count = (server.arg("timeout").toInt() * 2);
      }
      if (server.arg("brightness") == "0"){
        matrix_brightness = 0;
      } else {
        matrix_brightness = (server.arg("brightness").toInt());
      }
      matrix_display->setBrightness8(matrix_brightness);
      textcolor = server.arg("textcolor");
      textcolor.replace("%23", "#");
      playback = server.arg("playback");
      screensaver = server.arg("screensaver");
      accentcolor = server.arg("accentcolor");
      accentcolor.replace("%23", "#");
      String tz = server.arg("timezone");
      tz.replace("%2F", "/");
      tz.toCharArray(timezone, sizeof(timezone));
      if(server.arg("twelvehour") == "true"){
        twelvehour = true;
      } else {
        twelvehour = false;
      }
      
      // Write secrets.json
      StaticJsonDocument<512> doc;
      doc["ssid"] = server.arg("ssid");
      doc["password"] = server.arg("password");
      doc["timeout"] = (server.arg("timeout").toInt() * 2);
      doc["brightness"] = server.arg("brightness").toInt();
      doc["textcolor"] = textcolor;
      doc["playback"] = playback;
      doc["screensaver"] = screensaver;
      doc["accentcolor"] = accentcolor;
      doc["timezone"] = timezone;
      doc["twelvehour"] = twelvehour;
      File config_file = LittleFS.open(config_filename, FILE_WRITE);
      if (!config_file) {
        returnHTTPError(500, "Failed to open config file for writing");
      }
      serializeJson(doc, config_file);
      html =
        "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
        "<head>"
        "<title>Settings Saved</title>"
        + style +
        "</head>"
        "<body>"
        "<form>"
        "<p>Settings saved, you must reboot for WiFi and timezone changes to take effect.</p>"
        "<input id='back-button' type=\"button\" class=btn onclick=\"location.href='/settings';\" value=\"Back\" />"
        "<input type=\"button\" class=cautionbtn onclick=\"location.href='/reboot';\" value=\"Reboot\" />"
        "</form>"
        "</body>"
        "</html>";
    }
    returnHTML(html);
  } else {
    returnHTTPError(405, "Method Not Allowed");
  }
} /* handleSettings() */

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
      "<h1><a href='" + releases + "'>OTA Update</a></h1>"
      "<a href='" + releases + "'><div id='latestversion'></div></a><br>"
      "<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
      "<label id='file-input' for='file'>   Choose file...</label>"
      "<input id='sub-button' type='submit' class=actionbtn value='Update'>"
      "<br><br>"
      "<div id='prg'></div>"
      "<br><div id='prgbar'><div id='bar'></div></div><br>"
      "<input id='back-button' type=\"button\" class=btn onclick=\"location.href='/';\" value=\"Back\" />"
      "</form>"
      "<script>"
      "fetch('" + latest_url + "')"
      ".then(function(response) {"
      "response.text().then(function(text) {"
      "latestVersion = parseFloat(text);"
      "if (latestVersion > parseFloat(" + VERSION + ")){"
      "document.getElementById('latestversion').textContent = \"Update Available: \" + latestVersion;"
      "}"
      "});"
      "});"
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
    returnHTTPError(405, "Method Not Allowed");
  }
} /* handleOTA() */

void handleUpdate(){
  // curl -F 'file=@web2rgbmatrix.ino.bin' http://rgbmatrix.local/update
  client_ip = {0,0,0,0};
  LittleFS.remove(gif_filename);
  sd_filename = "";
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    showTextLine("OTA Update Started");
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // Start with max available size
      Update.printError(DBG_OUTPUT_PORT);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Flashing firmware to ESP32
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(DBG_OUTPUT_PORT);
      showTextLine("OTA Update Error");
    } else {
      showTextLine("OTA Update: " + String((Update.progress()*100)/Update.size()) + "%");
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      showTextLine("OTA Update Success");
    } else {
      Update.printError(DBG_OUTPUT_PORT);
      showTextLine("OTA Update Error");
    }
  }
} /* handleUpdate() */

void handleFileList() {
  if(card_mounted){
    if (!server.hasArg("dir") && !server.hasArg("file")) {
      returnHTTPError(500, "BAD ARGS");
      return;
    }
    if (server.hasArg("dir")) {
      String path = server.arg("dir");
      File root = SD.open(path);
      path = String();

      String html = 
        "<html>"
        "<head>"
        "<title>web2rgbmatrix - File Browser</title>"
        + style +
        "</head>"
        "<body>"
        "<form>"
        "<h1>File Browser:</h1>"
        "<h2>" + server.arg("dir") + "</h2>"
        "<p><table>";
      server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      server.sendHeader("Pragma", "no-cache");
      server.sendHeader("Expires", "-1");
      server.setContentLength(CONTENT_LENGTH_UNKNOWN);
      // Begin chunked transfer
      server.send(200, "text/html", "");
      server.sendContent(html);
      if(root.isDirectory()){
        if (server.arg("dir") != "/"){
          String parent = server.arg("dir");
          parent.replace(String(root.name()),"");
          if (parent != "/" && parent.endsWith("/")){
            parent.remove(parent.length() - 1, 1);
          }
          server.sendContent("<tr><td><a href=\"/list?dir=" + parent + "\">../</a></td></tr>");
        }
        File file = root.openNextFile();
        while(file){
          if (!String(file.name()).startsWith(".")) {
            server.sendContent("<tr><td><a href=\"/list?" + String((file.isDirectory()) ? "dir=" : "file=") + String(file.path()) + "\">" + String(file.name()) + String((file.isDirectory()) ? "/" : "") +  "</a></td></tr>");
          }
          file = root.openNextFile();
        }
      }
      server.sendContent("</table></p><input id='back-button' type=\"button\" class=btn onclick=\"location.href='/';\" value=\"Back\" /></form></body></html>");
      server.sendContent(F("")); // this tells web client that transfer is done
      server.client().stop();
    } else if (server.hasArg("file")) {
      String path = server.arg("file");
      File file = SD.open(path);
      path = String();
      if (!file) {
        returnHTTPError(404, "File Not Found");
        return;
      }
      String html = 
        "<html>"
        "<head>"
        "<title>web2rgbmatrix - File Browser</title>"
        + style +
        "</head>"
        "<body>"
        "<form>"
        "<h1>File Browser:</h1>"
        "<h3>" + server.arg("file") + "</h3>"
        "<table><p>"
        "<tr><td>Image</td><td><img src=\"" + server.arg("file") + "\" /></td></tr>"
        "<tr><td>Size</td><td>" + ((file.size() >= 1024) ? String(file.size() / 1024) + "KB" : String(file.size()) + "B") + "</td></tr>"
        "</table></p>"
        "<input id='play-button' type=\"button\" class=actionbtn onclick=\"location.href='/sdplay?file=" + server.arg("file") + "';\" value=\"Play\" />"
        "<input id='delete-button' type=\"button\" class=cautionbtn onclick=\"location.href='/delete?file=" + server.arg("file") + "';\" value=\"Delete\" />"
        "<input id='back-button' type=\"button\" class=btn onclick=\"history.back()\" value=\"Back\" />"
        "</form></body></html>";
      file.close();
      returnHTML(html);
    }
  } else {
    returnHTTPError(500, "SD Card Not Mounted");
  }
} /* handleFileList() */

void handleFileDelete() {
  if(card_mounted){
    if (!server.hasArg("file")) {
      returnHTTPError(500, "BAD ARGS");
      return;
    }

    File file = SD.open(server.arg("file"));
    if (!file) {
      returnHTTPError(404, "File Not Found");
      return;
    }
    SD.remove(server.arg("file"));

    String html = 
      "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
      "<head>"
      "<title>web2rgbmatrix - File Delete</title>"
      + style +
      "</head>"
      "<body>"
      "<form>"
      "<h1>File Deleted: " + server.arg("file") + "</h1>"
      "<input id='back-button' type=\"button\" class=btn onclick=\"window.history.go(-2)\" value=\"Back\" />"
      "</form></body></html>";
    returnHTML(html);
  } else {
    returnHTTPError(500, "SD Card Not Mounted");
  }
} /* handleFileDelete() */

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
        "<div>"
        "<label for=\"animated\" class=\"chkboxlabel\">"
        "<input type=\"checkbox\" name=\"animated\" id=\"animated\">"
        " Animated GIF</label>"
        "</div>"        
        "<input id='sub-button' type='submit' class=actionbtn value='Upload'>"
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
        "url = '/upload';"
        "if (document.getElementById('animated').checked) {"
        "url = '/upload?animated=true';"
        "}"
        "var form = $('#upload_form')[0];"
        "var data = new FormData(form);"
        "$.ajax({"
        "url: url,"
        "type: 'POST',"
        "data: data,"
        "contentType: false,"
        "processData: false,"
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
        "var folder = " + String(gif_folder) + ";"
        "if (document.getElementById('animated').checked) {"
        "folder = " + String(animated_gif_folder) + ";"
        "}"
        "$('#prg').html('Upload Success, click GIF to play<br><a href=\"/sdplay?file=' + folder + fileName.charAt(0).toUpperCase() + '/' + fileName +'\"><img src=\"' + folder + fileName.charAt(0) + '/' + fileName + '\"></a>');"
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
    returnHTTPError(405, "Method Not Allowed");
  }
} /* handleSD() */

void handleUpload() {
  if (card_mounted){
    HTTPUpload& uploadfile = server.upload();
    if(uploadfile.status == UPLOAD_FILE_START) {
      String filename = String(uploadfile.filename);
      String folder = String(gif_folder);
      if (server.arg("animated") != ""){
        folder = String(animated_gif_folder);
      }
      String letter_folder(filename.charAt(0));
      letter_folder.toUpperCase();
      if(!filename.startsWith("/")) filename = folder + String(letter_folder) + "/" + String(uploadfile.filename);
      SD.remove(filename);
      upload_file = SD.open(filename, FILE_WRITE);
      filename = String();
    } else if (uploadfile.status == UPLOAD_FILE_WRITE) {
      if(upload_file) upload_file.write(uploadfile.buf, uploadfile.currentSize);
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
  static long contentLength = 0;
  HTTPUpload& uploadfile = server.upload();
  if(uploadfile.status == UPLOAD_FILE_START) {
    contentLength = server.header("Content-Length").toInt();
    if (contentLength > (LittleFS.totalBytes() - LittleFS.usedBytes())) {
      DBG_OUTPUT_PORT.println("File too large: " + String(contentLength) + " > " + String(LittleFS.totalBytes() - LittleFS.usedBytes()));
      returnHTTPError(500, "File too large");
    }
    LittleFS.remove(gif_filename);
    upload_file = LittleFS.open(gif_filename, FILE_WRITE);
  } else if (uploadfile.status == UPLOAD_FILE_WRITE) {
    if(upload_file) upload_file.write(uploadfile.buf, uploadfile.currentSize);
  } else if (uploadfile.status == UPLOAD_FILE_END) {
    if(upload_file) {                                    
      upload_file.close();
      client_ip = server.client().remoteIP();
      ping_fail = 0;
      returnHTML("SUCCESS");
      sd_filename = "";
      tty_client = false;
      showGIF(gif_filename,false);
    } else {
      returnHTTPError(500, "Couldn't create file");
    }
  }
} /* handleRemotePlay() */

void handleLocalPlay(){
  // To play a GIF from SD card with curl
  // curl http://rgbmatrix.local/localplay?file=MENU
  if (server.method() == HTTP_GET) {
    if (card_mounted){
      if (server.arg("file") != "") {
        String start_html =
          "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
          "<head>"
          "<title>Local Play</title>"
          + style +
          "</head>"
          "<body>"
          "<form>"
          "<p>";
        String end_html =
          "</p>"
          "<input id='back-button' type=\"button\" class=btn onclick=\"location.href='/';\" value=\"Back\" />"
          "</form>"
          "</body>"
          "</html>";
        client_ip = server.client().remoteIP();
        ping_fail = 0;
        tty_client = false;
        LittleFS.remove(gif_filename);
        String letter_folder(server.arg("file").charAt(0));
        letter_folder.toUpperCase();
        bool gif_found = false;
        if (playback != "Static") {
          // Check for and play animated GIF
          String agif_fullpath = String(animated_gif_folder) + String(letter_folder) + "/" + server.arg("file") + ".gif";
          const char *agif_requested_filename = agif_fullpath.c_str();
          if (SD.exists(agif_requested_filename)) {
            sd_filename = agif_fullpath;
            returnHTML(start_html + "Displaying GIF: " + sd_filename + end_html);
            showGIF(agif_requested_filename, true);
            gif_found = true;
          }
        }
        if (playback != "Animated") {
          // Check for and play static GIF
          String fullpath = String(gif_folder) + String(letter_folder) + "/" + server.arg("file") + ".gif";
          const char *requested_filename = fullpath.c_str();
          if (SD.exists(requested_filename)) {
            sd_filename = fullpath;
            returnHTML(start_html + "Displaying GIF: " + sd_filename + end_html);
            showGIF(requested_filename, true);
            gif_found = true;
          }
        }
        if(!gif_found) {
          sd_filename = "";
          returnHTTPError(404, "File Not Found");
          showTextLine(server.arg("file"));
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

void handleSDPlay(){
  // To play a GIF from SD card with curl
  // curl http://rgbmatrix.local/sdplay?file=/gifs/M/MENU.gif
  if (server.method() == HTTP_GET) {
    if (card_mounted){
      if (server.arg("file") != "") {
        String start_html =
          "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
          "<head>"
          "<title>SD Play</title>"
          + style +
          "</head>"
          "<body>"
          "<form>"
          "<p>";
        String end_html =
          "</p>"
           "<input id='back-button' type=\"button\" class=btn onclick=\"location.href='/';\" value=\"Back\" />"
          "</form>"
          "</body>"
          "</html>";
        client_ip = server.client().remoteIP();
        ping_fail = 0;
        tty_client = false;
        LittleFS.remove(gif_filename);
        // Check for and play animated GIF
        const char *requested_filename = server.arg("file").c_str();
        if (SD.exists(server.arg("file"))) {
          returnHTML(start_html + "Displaying SD File: " + server.arg("file") + end_html);
          sd_filename = server.arg("file");
          showGIF(requested_filename, true);
        } else {
          returnHTTPError(404, start_html + "File Not Found: " + server.arg("file") + end_html);
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
} /* handleSDPlay() */

void handleText(){
  // To display a line of text with curl
  // curl http://rgbmatrix.local/text?line=Text
  if (server.method() == HTTP_GET) {
    if (server.arg("line") != "") {
      String start_html =
        "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
        "<head>"
        "<title>SD Play</title>"
        + style +
        "</head>"
        "<body>"
        "<form>"
        "<p>";
      String end_html =
        "</p>"
        "<input id='back-button' type=\"button\" class=btn onclick=\"location.href='/';\" value=\"Back\" />"
        "</form>"
        "</body>"
        "</html>";
        client_ip = server.client().remoteIP();
        ping_fail = 0;
        tty_client = false;
        LittleFS.remove(gif_filename);
        showTextLine(server.arg("line"));
        returnHTML(start_html + "Displaying Text: " + server.arg("line") + end_html);
      } else {
        returnHTTPError(405, "Method Not Allowed");
      }
  } else {
    returnHTTPError(405, "Method Not Allowed");
  }
} /* handleText() */

void handleVersion(){
  returnHTML(VERSION);
} /* handleVersion() */

void handleClear(){
  matrix_display->clearScreen();
  matrix_display->setBrightness8(matrix_brightness);
  client_ip = {0,0,0,0};
  sd_filename = "";
  config_display_on = false;
  tty_client = false;
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
    "<input id='back-button' type=\"button\" class=btn onclick=\"location.href='/';\" value=\"Back\" />"
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
    "<input id='back-button' type=\"button\" class=btn onclick=\"location.href='/';\" value=\"Back\" />"
    "</form>"
    "</body>"
    "</html>";
  returnHTML(html);
  matrix_display->clearScreen();
  LittleFS.remove(gif_filename);
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

void checkClientTimeout() {
  // Check if client who requested core image has gone away, if so clear screen
  if(client_ip != no_ip){
    if (ping_fail_count != 0){
      if (ping_fail <= ping_fail_count){
        if (millis() - last_seen >= 30*1000UL){
          last_seen = millis();
          bool success = Ping.ping(client_ip, 1);
          if(!success){
            DBG_OUTPUT_PORT.println("Ping failed");
            ping_fail = ping_fail + 1;
          } else {
            DBG_OUTPUT_PORT.println("Ping success");
            ping_fail = 0;
          }
        }
      } else {
        // Client gone clear display
        DBG_OUTPUT_PORT.println("Client gone, clearing display and deleting the GIF.");
        matrix_display->clearScreen();
        matrix_display->setBrightness8(matrix_brightness);
        client_ip = {0,0,0,0};
        LittleFS.remove(gif_filename);
        sd_filename = "";
      }
    }
  } else {
    // Screen Saver
    if (config_display_on == false && tty_client == false){
      if (screensaver == "Clock") clockScreenSaver(); // Clock
      if (screensaver == "Plasma") plasmaScreenSaver(); // Plasma
      if (screensaver == "Starfield") starfieldScreenSaver(); // Starfield
      if (screensaver == "Toasters") toasterScreenSaver(); // Toasters
    }
  }
} /* checkClientTimeout() */

void checkSerialClient() {
  if (DBG_OUTPUT_PORT.available()) {
    new_command = DBG_OUTPUT_PORT.readStringUntil('\n');
    new_command.trim();
  }  
  if (new_command != current_command) {
    if (new_command.endsWith("QWERTZ"));
    else if (new_command.startsWith("CMD"));
    else if (new_command == "cls");
    else if (new_command == "sorg");
    else if (new_command == "bye");
    else {
      tty_client = true;
      sd_filename = "";
      LittleFS.remove(gif_filename);
      client_ip = {0,0,0,0};
      bool gif_found = false;
      if (card_mounted) {
        if (playback != "Static") {
          // Check for and play animated GIF
          String agif_fullpath = String(animated_gif_folder) + new_command.charAt(0) + "/" + new_command + ".gif";
          const char *agif_requested_filename = agif_fullpath.c_str();
          if (SD.exists(agif_requested_filename)) {
            sd_filename = agif_fullpath;
            showGIF(agif_requested_filename, true);
            gif_found = true;
          }
        }
        if (playback != "Animated") {
          // Check for and play static GIF
          String fullpath = String(gif_folder) + new_command.charAt(0) + "/" + new_command + ".gif";
          const char *requested_filename = fullpath.c_str();
          if (SD.exists(requested_filename)) {
            sd_filename = fullpath;
            showGIF(requested_filename, true);
            gif_found = true;
          }
        }
      }
      if(!gif_found) {
        sd_filename = "";
        showTextLine(new_command);
      }
    }
    current_command = new_command;
  }
} /* checkSerialClient() */

void displaySetup() {
  HUB75_I2S_CFG mxconfig(
    panelResX,           // module width
    panelResY,           // module height
    panels_in_X_chain    // Chain length
  );

  mxconfig.gpio.e = E;
  mxconfig.gpio.r1 = R1;
  mxconfig.gpio.r2 = R2;
  if (digitalRead(ALT) == HIGH) {
    mxconfig.gpio.b1 = B1;
    mxconfig.gpio.b2 = B2;
    mxconfig.gpio.g1 = G1;
    mxconfig.gpio.g2 = G2;
  } else {
    mxconfig.gpio.b1 = G1;
    mxconfig.gpio.b2 = G2;
    mxconfig.gpio.g1 = B1;
    mxconfig.gpio.g2 = B2;    
  }
  mxconfig.clkphase = false;

  matrix_display = new MatrixPanel_I2S_DMA(mxconfig);
  matrix_display->begin();
  matrix_display->clearScreen();
  matrix_display->setBrightness8(matrix_brightness);
} /* displaySetup() */

void showTextLine(String text){
  int char_width = 6;
  int char_heigth = 8;
  int x = (totalWidth - (text.length() * char_width)) / 2;
  int y = (totalHeight - char_heigth) / 2;

  String hexcolor = textcolor;
  hexcolor.replace("#","");
  char charbuf[8];
  hexcolor.toCharArray(charbuf,8);
  long int rgb=strtol(charbuf,0,16);
  byte r=(byte)(rgb>>16);
  byte g=(byte)(rgb>>8);
  byte b=(byte)(rgb);

  matrix_display->clearScreen();
  matrix_display->setBrightness8(matrix_brightness);
  matrix_display->setTextColor(matrix_display->color565(r, g, b));
  matrix_display->setCursor(x, y);
  matrix_display->println(text);
} /* showTextLine() */

void showText(String text){
  String hexcolor = textcolor;
  hexcolor.replace("#","");
  char charbuf[8];
  hexcolor.toCharArray(charbuf,8);
  long int rgb=strtol(charbuf,0,16);
  byte r=(byte)(rgb>>16);
  byte g=(byte)(rgb>>8);
  byte b=(byte)(rgb);

  matrix_display->clearScreen();
  matrix_display->setBrightness8(matrix_brightness);
  matrix_display->setTextColor(matrix_display->color565(r, g, b));
  matrix_display->setCursor(0, 0);
  matrix_display->println(text);
} /* showText() */

// Copy a horizontal span of pixels from a source buffer to an X,Y position
// in matrix back buffer, applying horizontal clipping. Vertical clipping is
// handled in GIFDraw() below -- y can safely be assumed valid here.
void span(uint16_t *src, int16_t x, int16_t y, int16_t width) {
  int16_t gifWidth = width;
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
    int16_t xOffset = (totalWidth - gif.getCanvasWidth()) / 2;
    matrix_display->drawPixel((x++) + xOffset, y, *src++);
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
  DBG_OUTPUT_PORT.print("Playing gif: ");DBG_OUTPUT_PORT.println(fname);
  gif_file = LittleFS.open(fname);
  if (gif_file) {
    *pSize = gif_file.size();
    return (void *)&gif_file;
  }
  return NULL;
} /* GIFOpenFile() */

void * GIFSDOpenFile(const char *fname, int32_t *pSize) {
  DBG_OUTPUT_PORT.print("Playing gif from SD: ");DBG_OUTPUT_PORT.println(fname);
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

void showGIF(const char *name, bool sd) {
  config_display_on = false;
  matrix_display->clearScreen();
  matrix_display->setBrightness8(matrix_brightness);
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
} /* showGIF() */

void drawXbm565(int x, int y, int width, int height, const char *xbm, uint16_t color = 0xffff) {
  if (width % 8 != 0) {
    width =  ((width / 8) + 1) * 8;
  }
  for (int i = 0; i < width * height; i++ ) {
    unsigned char charColumn = pgm_read_byte(xbm + i);
    int swap = 0;
    for (int j = 7; j >= 0; j--) {
      //int targetX = (i * 8 + j) % width + x;
      int targetX = (i * 8 + swap) % width + x;
      int targetY = (8 * i / (width)) + y;
      if (swap >= 7){
        swap = 0;
      } else {
        swap++;
      }
      if (bitRead(charColumn, j)) {
        matrix_display->drawPixel(targetX, targetY, color);
      }
    }
  }
} /* drawXbm565() */

void showStatus(){
  String hexcolor = accentcolor;
  hexcolor.replace("#","");
  char charbuf[8];
  hexcolor.toCharArray(charbuf,8);
  long int rgb=strtol(charbuf,0,16);
  byte r=(byte)(rgb>>16);
  byte g=(byte)(rgb>>8);
  byte b=(byte)(rgb);
  String display_string = "Wifi: " + wifi_mode +"\n" + my_ip.toString() + "\nrgbmatrix.local\nSD: " + sd_status;
  matrix_display->clearScreen();
  matrix_display->setBrightness8(matrix_brightness);
  matrix_display->setTextColor(matrix_display->color565(r, g, b));
  matrix_display->setCursor(0, 0);
  matrix_display->print(display_string);
  drawXbm565((totalWidth - ICON_WIFI_WIDTH), (totalHeight - ICON_WIFI_HEIGHT), ICON_WIFI_HEIGHT, ICON_WIFI_WIDTH, icon_wifi_bmp, matrix_display->color565(r, g, b));
} /* showStatus() */

void plasmaScreenSaver() {
  for (int x = 0; x < (totalWidth); x++) {
    for (int y = 0; y < (totalHeight); y++) {
    int16_t v = 0;
    uint8_t wibble = sin8(time_counter);
    v += sin16(x * wibble * 3 + time_counter);
    v += cos16(y * (128 - wibble)  + time_counter);
    v += sin16(y * x * cos8(-time_counter) / 8);

    currentColor = ColorFromPalette(currentPalette, (v >> 8) + 127);
    matrix_display->drawPixelRGB888(x, y, currentColor.r, currentColor.g, currentColor.b);
    }
  }

  ++time_counter;
  ++cycles;

  if (cycles >= 1024) {
    time_counter = 0;
    cycles = 0;
    currentPalette = palettes[random(0,sizeof(palettes)/sizeof(palettes[0]))];
  }
} /* plasmaScreenSaver() */

void starfieldScreenSaver() {
  String hexcolor = accentcolor;
  hexcolor.replace("#","");
  char charbuf[8];
  hexcolor.toCharArray(charbuf,8);
  long int rgb=strtol(charbuf,0,16);
  byte r=(byte)(rgb>>16);
  byte g=(byte)(rgb>>8);
  byte b=(byte)(rgb);

  bufferClear(matrix_buffer);

  int origin_x = (totalWidth) / 2;
  int origin_y = (totalHeight) / 2;

  // Iterate through the stars reducing the z co-ordinate in order to move the
  // star closer.
  for (int i = 0; i < starCount; ++i) {
    stars[i][2] -= 0.19;
    // if the star has moved past the screen (z < 0) reposition it far away
    // with random x and y positions.
    if (stars[i][2] <= 0) {
      stars[i][0] = getRandom(-25, 25);
      stars[i][1] = getRandom(-25, 25);
      stars[i][2] = maxDepth;
    }

    // Convert the 3D coordinates to 2D using perspective projection.
    double k = (totalWidth) / stars[i][2];
    int x = static_cast<int>(stars[i][0] * k + origin_x);
    int y = static_cast<int>(stars[i][1] * k + origin_y);

    // Draw the star (if it is visible in the screen).
    // Distant stars are smaller than closer stars.
    if ((0 <= x and x < (totalWidth)) 
      and (0 <= y and y < (totalHeight))) {
      int size = (1 - stars[i][2] / maxDepth) * 4;

      for (int xplus = 0; xplus < size; xplus++) {
        for (int yplus = 0; yplus < size; yplus++) {
          if ((((y + yplus) * (totalWidth) + (x + xplus)) < (totalWidth) * (totalHeight))) {
            matrix_buffer[(y + yplus) * (totalWidth) + (x + xplus)].r=r;
            matrix_buffer[(y + yplus) * (totalWidth) + (x + xplus)].g=g;
            matrix_buffer[(y + yplus) * (totalWidth) + (x + xplus)].b=b;
          }
        }
      }
      matrixFill(matrix_buffer);
    }
  }
} /* starfieldScreenSaver() */

void clockScreenSaver() {
  unsigned long now = millis();
  if (now > oneSecondLoopDue) {
    // We can call this often, but it will only
    // update when it needs to
    setMatrixTime();
    showColon = !showColon;

    // To reduce flicker on the screen we stop clearing the screen
    // when the animation is finished, but we still need the colon to
    // to blink
    if (finishedAnimating) {
      handleColonAfterAnimation();
    }
    oneSecondLoopDue = now + 1000;
  }
  now = millis();
  if (now > animationDue) {
    animationHandler();
    animationDue = now + animationDelay;
  }
}

void toasterScreenSaver() {
  String hexcolor = accentcolor;
  hexcolor.replace("#","");
  char charbuf[8];
  hexcolor.toCharArray(charbuf,8);
  long int rgb=strtol(charbuf,0,16);
  byte r=(byte)(rgb>>16);
  byte g=(byte)(rgb>>8);
  byte b=(byte)(rgb);

  uint8_t i, f;
  int16_t x, y;
  boolean resort = false;     // By default, don't re-sort depths

  matrix_display->clearScreen();     // Start drawing next frame
  matrix_display->setBrightness8(matrix_brightness);

  for(i=0; i<N_FLYERS; i++) { // For each flyer...

    // First draw each item...
    f = (flyer[i].frame == 255) ? 4 : (flyer[i].frame++ & 3); // Frame #
    x = flyer[i].x / 16;
    y = flyer[i].y / 16;
    drawXbm565(x, y, 32, 32, (const char *)mask[f], 0x0000);
    drawXbm565(x, y, 32, 32, (const char *)img[f], matrix_display->color565(r, g, b));

    // Then update position, checking if item moved off screen...
    flyer[i].x -= flyer[i].depth * 2; // Update position based on depth,
    flyer[i].y += flyer[i].depth;     // for a sort of pseudo-parallax effect.
    if((flyer[i].y >= (64*16)) || (flyer[i].x <= (-32*16))) { // Off screen?
      if(random(7) < 5) {         // Pick random edge; 0-4 = top
        flyer[i].x = random(160) * 16;
        flyer[i].y = -32         * 16;
      } else {                    // 5-6 = right
        flyer[i].x = 128         * 16;
        flyer[i].y = random(64)  * 16;
      }
      flyer[i].frame = random(3) ? random(4) : 255; // 66% toaster, else toast
      flyer[i].depth = 10 + random(16);
      resort = true;
    }
  }
  // If any items were 'rebooted' to new position, re-sort all depths
  if(resort) qsort(flyer, N_FLYERS, sizeof(struct Flyer), compare);
  delay(50);
} /* toasterScreenSaver() */

// Flyer depth comparison function for qsort()
static int compare(const void *a, const void *b) {
  return ((struct Flyer *)a)->depth - ((struct Flyer *)b)->depth;
} /* compare */

// This method is for controlling the tetris library draw calls
void animationHandler() {
  // Not clearing the display and redrawing it when you
  // dont need to improves how the refresh rate appears
  if (!finishedAnimating) {
    matrix_display->fillScreen(0);
    if (twelvehour) {
      // Place holders for checking are any of the tetris objects
      // currently still animating.
      bool tetris1Done = false;
      bool tetris2Done = false;
      bool tetris3Done = false;

      tetris1Done = tetris.drawNumbers(-6 + tetrisXOffset, 10 + tetrisYOffset, showColon);
      tetris2Done = tetris2.drawText(56 + tetrisXOffset, 9 + tetrisYOffset);

      // Only draw the top letter once the bottom letter is finished.
      if (tetris2Done) {
        tetris3Done = tetris3.drawText(56 + tetrisXOffset, -1 + tetrisYOffset);
      }

      finishedAnimating = tetris1Done && tetris2Done && tetris3Done;

    } else {
      finishedAnimating = tetris.drawNumbers(2 + tetrisXOffset, 10 + tetrisYOffset, showColon);
    }
    matrix_display->flipDMABuffer();
  }
} /* animationHandler() */

void setMatrixTime() {
  String timeString = "";
  String AmPmString = "";
  if (twelvehour) {
    // Get the time in format "1:15" or 11:15 (12 hour, no leading 0)
    // Check the EZTime Github page for info on
    // time formatting
    timeString = myTZ.dateTime("g:i");

    //If the length is only 4, pad it with
    // a space at the beginning
    if (timeString.length() == 4) {
      timeString = " " + timeString;
    }

    //Get if its "AM" or "PM"
    AmPmString = myTZ.dateTime("A");
    if (lastDisplayedAmPm != AmPmString) {
      lastDisplayedAmPm = AmPmString;
      // Second character is always "M"
      // so need to parse it out
      tetris2.setText("M", forceRefresh);

      // Parse out first letter of String
      tetris3.setText(AmPmString.substring(0, 1), forceRefresh);
    }
  } else {
    // Get time in format "01:15" or "22:15"(24 hour with leading 0)
    timeString = myTZ.dateTime("H:i");
  }

  // Only update Time if its different
  if (lastDisplayedTime != timeString) {
    lastDisplayedTime = timeString;
    tetris.setTime(timeString, forceRefresh);

    // Must set this to false so animation knows
    // to start again
    finishedAnimating = false;
  }
} /* setMatrixTime() */

void handleColonAfterAnimation() {
  // It will draw the colon every time, but when the colour is black it
  // should look like its clearing it.
  uint16_t colour =  showColon ? tetris.tetrisWHITE : tetris.tetrisBLACK;
  // The x position that you draw the tetris animation object
  int x = twelvehour ? -6 : 2;
  x = x + tetrisXOffset;
  // The y position adjusted for where the blocks will fall from
  // (this could be better!)
  int y = 10 + tetrisYOffset - (TETRIS_Y_DROP_DEFAULT * tetris.scale);
  tetris.drawColon(x, y, colour);
  matrix_display->flipDMABuffer();
} /* handleColonAfterAnimation() */

int getRandom(int lower, int upper) {
    /* Generate and return a  random number between lower and upper bound */
    return lower + static_cast<int>(rand() % (upper - lower + 1));
} /* getRandom() */

void bufferClear(CRGB *buf){
  memset(buf, 0x00, ((totalWidth) * (totalHeight)) * sizeof(CRGB)); // flush buffer to black  
} /* bufferClear() */

void matrixFill(CRGB *leds){
  uint16_t y = (totalHeight);
  do {
    --y;
    uint16_t x = (totalWidth);
    do {
      --x;
        uint16_t _pixel = y * (totalWidth) + x;
        matrix_display->drawPixelRGB888( x, y, leds[_pixel].r, leds[_pixel].g, leds[_pixel].b);
    } while(x);
  } while(y);
} /* matrixFill() */