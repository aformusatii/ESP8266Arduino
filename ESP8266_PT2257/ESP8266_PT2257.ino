#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUDP.h>
#include <WiFiConfig.h>
#include <Evc_pt2257.h>

#define SDA_PIN 12
#define SCL_PIN 13

const char* host = "pt2257-01";

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

// Multicast declarations
IPAddress ipMulti(239, 0, 0, 1);
unsigned int portMulti = 6000;      // local port to listen on

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

void indexPage() {
  String message = "<!doctype html>";
  message += "<html lang=\"en\">";

  message += "<head>";
  message += "<title>ESP8266 Volume Controller (PT2257)</title>";
  message += "</head>";

  message += "<body>";

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

  // message += "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  
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

void setupHTTPActions() {
  server.on("/", indexPage);

  server.on("/setVolume", [](){

    if (server.hasArg("value")) {
      uint8_t value = (uint8_t) server.arg("value").toInt();
  
      if (evc_setVolumeLevel(value)) {
        server.send(500, "text/plain", "I2C error.");  
      } else {
        server.send(200, "text/plain", "volume set");  
      }

    } else {
      server.send(400, "text/plain", "Bad Request, no 'value' parameter.");  
    }
    
  });

  server.on("/incVolume", [](){
    uint8_t delta = 1;

    if (server.hasArg("delta")) {
      delta = (uint8_t) server.arg("delta").toInt();
    }

    if (evc_incVolumeLevel(delta)) {
      server.send(500, "text/plain", "I2C error.");  
    } else {
      server.send(200, "text/plain", "volume increased");  
    }

  });

  server.on("/decVolume", [](){
    uint8_t delta = 1;

    if (server.hasArg("delta")) {
      delta = (uint8_t) server.arg("delta").toInt();
    }

    if (evc_decVolumeLevel(delta)) {
      server.send(500, "text/plain", "I2C error.");  
    } else {
      server.send(200, "text/plain", "volume decreased");
    }

  });

  // Not found handler
  server.onNotFound(handleNotFound);
}

void setupGPIO() {
}

void setup(void) {
  setupGPIO();

  evc_init(SDA_PIN, SCL_PIN);
  
  Serial.begin(115200);

  WiFi.begin(ssid, password);

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

  MDNS.begin(host);

  setupHTTPActions();

  httpUpdater.setup(&server);

  server.begin();

  MDNS.addService("http", "tcp", 80);
  
  Serial.println("HTTP server started");
  Serial.println("v0.1");
}

// ============= Main Loop =============
void loop(void) {
  server.handleClient();
}
