#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiConfig.h>
#include <ESP8266HTTPClient.h>
#include <RCSwitch.h>

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

RCSwitch mySwitch = RCSwitch();

#define WIFI_CHECK_INTERVAL_MS 2000

volatile unsigned long wifi_check_timer = 0;
volatile char wifi_last_status = 255;
volatile char wifi_setup_complete = false;

void indexPage() {
  String message = "<!doctype html>";
  message += "<html lang=\"en\">";
  message += "<head>";
  message += "<title>ESP8266 Balcony LED Controller v0.1</title>";
  message += "</head>";

  message += "<body>";
  
  message += "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  message += "--------------------------------------------------------------<br/>";
  
  uint32_t realSize = ESP.getFlashChipRealSize();
  uint32_t ideSize = ESP.getFlashChipSize();
  uint32_t chipId = ESP.getFlashChipId();
  uint32_t chipSpeed = ESP.getFlashChipSpeed();
  FlashMode_t ideMode = ESP.getFlashChipMode();

  char buf[255];
  sprintf(buf, "Flash chip id: %08X<br/>Flash chip speed: %u<br/>Flash chip size: %u<br/>Flash ide mode: %s<br/>Flash real size: %u<br/>",
    chipId, 
    chipSpeed,
    ideSize,
    (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"),
    realSize);

  message += buf;

  message += "--------------------------------------------------------------<br/>";

  /*
    Signal Strength TL;DR   Required for
    -30 dBm Amazing Max achievable signal strength. The client can only be a few feet from the AP to achieve this. Not typical or desirable in the real world.  N/A
    -67 dBm Very Good Minimum signal strength for applications that require very reliable, timely delivery of data packets. VoIP/VoWiFi, streaming video
    -70 dBm Okay  Minimum signal strength for reliable packet delivery. Email, web
    -80 dBm Not Good  Minimum signal strength for basic connectivity. Packet delivery may be unreliable.  N/A
    -90 dBm Unusable  Approaching or drowning in the noise floor. Any functionality is highly unlikely. N/A
  */
  message += "RSSI: ";
  message.concat(WiFi.RSSI());
  message += "<br/>";

  message += "BSSID MAC: ";
  message.concat(WiFi.BSSIDstr());
  message += "<br/>";

  message += "WIFI MAC: ";
  message.concat(WiFi.macAddress());
  message += "<br/>";

  message += "--------------------------------------------------------------<br/>";
  
  message += "</body>";
  
  message += "</html>";
  server.send(200, "text/html", message);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setupHttpHandlers() {
  server.on("/", indexPage);
  server.onNotFound(handleNotFound);
}

void setupGPIO() {
}

void setup(void) {
  setupGPIO();
  
  Serial.begin(115200);

  Serial.println("Start WIFI...");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  mySwitch.enableReceive(D8);
}

void setupAfterWiFiConnected(void) {
  setupHttpHandlers();
  
  httpUpdater.setup(&server);
  server.begin();
  
  Serial.println("HTTP server started");
}

void handleWiFiSetup(void) {
  unsigned long current_time = millis();

  if ((current_time - wifi_check_timer) < WIFI_CHECK_INTERVAL_MS) {
    // not the time yet
    return;
  }

  wifi_check_timer = current_time;

  char status = WiFi.status();

  // inform only if wifi status changes
  if (status != wifi_last_status) {
    Serial.println("WIFI Status: " + String(WiFi.status()));
  }

  // Store last wifi status
  wifi_last_status = status;

  // kick off the setup if this is not complete and wifi just connected
  if (!wifi_setup_complete && (status == WL_CONNECTED)) {
    wifi_setup_complete = true;

    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    setupAfterWiFiConnected();
  }
}

void handleRFSwitch(void) {
  if (mySwitch.available()) {
    
    Serial.print("Received ");
    Serial.print( mySwitch.getReceivedValue() );
    Serial.print(" / ");
    Serial.print( mySwitch.getReceivedBitlength() );
    Serial.print("bit ");
    Serial.print("Protocol: ");
    Serial.println( mySwitch.getReceivedProtocol() );

    mySwitch.resetAvailable();
  }
}

void loop(void) {
  handleWiFiSetup();
  handleRFSwitch();

  if (wifi_setup_complete) {
    server.handleClient();
  }
}
