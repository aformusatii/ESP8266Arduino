#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUDP.h>

#define SERIAL_BUFFER_SIZE 500

const char* host = "DustSensor";
const char* ssid = "DEFENDOR";
const char* password = "****";

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

volatile bool udpConnected = false;

IPAddress udpRemoteIP(192, 168, 1, 116);
unsigned int udpRemotePort = 6001; 
unsigned int udpLocalPort = 6000;

char udpPacketBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;


char serialBuffer[SERIAL_BUFFER_SIZE];
unsigned int serialReadIndex = 0;
boolean serialReceived = false;

void indexPage() {
  String message = "<!doctype html>";
  message += "<html lang=\"en\">";
  message += "<head>";
  message += "<title>ESP8266 Dust Sensor</title>";
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

  message += "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  
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
  server.on("/test", [](){
    server.send(200, "text/plain", "test");
  });

  // Not found handler
  server.onNotFound(handleNotFound);
}

void setupGPIO() {
}

void setup(void) {
  setupGPIO();
  
  Serial.begin(250000, SERIAL_8O2);

  //WiFi.mode(WIFI_STA);

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

  udpConnected = connectUDP();

  MDNS.begin(host);

  setupHTTPActions();

  httpUpdater.setup(&server);

  server.begin();

  MDNS.addService("http", "tcp", 80);
  
  Serial.println("HTTP server started");
  Serial.println("v0.4");
}

// connect to UDP – returns true if successful or false if not
boolean connectUDP() {
    boolean state = false;

    Serial.println("");
    Serial.println("Connecting to UDP");

    if(Udp.begin(udpLocalPort) == 1) {
        Serial.println("UDP connection successful");
        state = true;
    } else {
        Serial.println("UDP connection failed");
    }

    return state;
}

// ============= Main Loop =============
void loop(void) {
  server.handleClient();
  handleSerial();
  handleUDP();
}

void handleSerial() {
  while (Serial.available()) {
    char inChar = (char) Serial.read();

    if (serialReadIndex < SERIAL_BUFFER_SIZE) {
      serialBuffer[serialReadIndex++] = inChar;      
    }

    if (inChar == 10) {
      serialReceived = true;
    }
  }

  if (serialReceived) {
    Udp.beginPacket(udpRemoteIP, udpRemotePort);

    char writeBuffer[serialReadIndex];
    memcpy(writeBuffer, serialBuffer, serialReadIndex);

    Udp.write(writeBuffer, serialReadIndex);
    Udp.endPacket();

    serialReadIndex = 0;
    serialReceived = false;
  }
}

void handleUDP() {
  if(udpConnected) {
    int packetSize = Udp.parsePacket();
    if(packetSize) {
        // read the packet into packetBufffer
        int len = Udp.read(udpPacketBuffer, UDP_TX_PACKET_MAX_SIZE);

        udpPacketBuffer[len - 1] = 13;
        
        for (int i = 0; i < len; i++) {
          Serial.print(udpPacketBuffer[i]);  
        }
    }
  }  
}
