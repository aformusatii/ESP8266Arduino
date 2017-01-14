#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUDP.h>
#include <NDIRZ16.h>

const char* host = "MHZ16-1";
const char* ssid = "DEFENDOR";
const char* password = "****";

volatile unsigned long timer_counter_1 = 0;

IPAddress udpRemoteIP(192, 168, 1, 116);
unsigned int udpRemotePort = 6002; 

WiFiUDP Udp;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

NDIRZ16 mySensor = NDIRZ16(&Serial);

void indexPage() {
  String message = "<!doctype html>";
  message += "<html lang=\"en\">";
  message += "<head>";
  message += "<title>ESP8266 MH-Z16 Sensor</title>";
  message += "</head>";

  message += "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

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

  // ============= Port A =============
  server.on("/switchOnA", [](){
    server.send(200, "text/plain", "switch on");
  });

  // Not found handler
  server.onNotFound(handleNotFound);
}

void setupGPIO() {
}

void setup(void) {
  setupGPIO();
  
  Serial.begin(9600);

  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  MDNS.begin(host);

  setupHTTPActions();

  httpUpdater.setup(&server);

  server.begin();

  MDNS.addService("http", "tcp", 80);  

  timer_counter_1 = millis() + 15000;
}

// ============= Main Loop =============
void loop(void) {
  server.handleClient();

  unsigned long currentTime = millis();

  if (timer_counter_1 < currentTime) {
    timer_counter_1 = currentTime + 1000;

    if (mySensor.measure()) {
        //Serial.print("CO2 Concentration is ");
        //Serial.print(mySensor.ppm);
        //Serial.println("ppm");

        char buffer[255];
        sprintf(buffer, "ppm=[%d] t=[%d]\n", mySensor.ppm, mySensor.temperature);
        sendData(buffer);
    } else {
        sendData("Failed to read sensor data\n");  
    }
  }
  
}

void sendData(char writeBuffer[], unsigned int len) {
    Udp.beginPacket(udpRemoteIP, udpRemotePort);
    Udp.write(writeBuffer, len);
    Udp.endPacket();
}

void sendData(char writeBuffer[]) {
    Udp.beginPacket(udpRemoteIP, udpRemotePort);
    Udp.write(writeBuffer);
    Udp.endPacket();
}

