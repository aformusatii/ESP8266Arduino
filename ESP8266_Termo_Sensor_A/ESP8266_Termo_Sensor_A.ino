#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiConfig.h>
#include <ESP8266HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

// GPIO where the DS18B20 is connected to
const int oneWireBus = 2;

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

void indexPage() {
  String message = "<!doctype html>";
  message += "<html lang=\"en\">";
  message += "<head>";
  message += "<title>ESP8266 Temperature Sensor A v0.1</title>";
  message += "</head>";

  message += "<body>";
  
  message += "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  message += "--------------------------------------------------------------<br/>";
  
  //message += "<a href=\"/switchOn\">switchOn</a><br/>";
  //message += "<a href=\"/switchOff\">switchOff</a><br/>";
  //message += "<a href=\"/switchToggle\">switchToggle</a><br/>";
  //message += "<a href=\"/setLevel\">setLevel</a><br/>";
  //message += "<a href=\"/status\">status</a><br/>";
  
  //message += "--------------------------------------------------------------<br/>";
  
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

void setup(void) {
  setupGPIO();
  
  Serial.begin(115200);

  // Start the DS18B20 sensor
  sensors.begin();
  
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  setupHttpHandlers();

  httpUpdater.setup(&server);

  server.begin();
  
  Serial.println("HTTP server started");  
}

void setupGPIO() {
}

void setupHttpHandlers() {
  server.on("/", indexPage);

  server.on("/getTemperature", [](){
    sensors.requestTemperatures();
    float temperatureC = sensors.getTempCByIndex(0);
    String content = String(temperatureC);
    server.send(200, "text/plain", content);
  });

  server.onNotFound(handleNotFound);  
}

void loop(void){
  server.handleClient();
}
