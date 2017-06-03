#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUDP.h>
#include <ESP8266HTTPClient.h>
#include <WiFiConfig.h>

#define DEVICE_ID      "2"

#define GPIO_POUT_R     14
#define GPIO_POUT_G     16
#define GPIO_POUT_B     12

const char* host = "rgb-controller"DEVICE_ID;

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

// UDP local port to listen on
unsigned int portMulti = 6001;

volatile bool udpConnected = false;
char udpPacketBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,

void indexPage() {
  String message = "<!doctype html>";
  message += "<html lang=\"en\">";
  message += "<head>";
  message += "<title>ESP8266 RGB Controller "DEVICE_ID"</title>";
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

  server.on("/setValue", [](){
    int valueR = server.arg("r").toInt();
    int valueG = server.arg("g").toInt();
    int valueB = server.arg("b").toInt();
    
    analogWrite(GPIO_POUT_R, valueR);
    analogWrite(GPIO_POUT_G, valueG);
    analogWrite(GPIO_POUT_B, valueB);
    
    server.send(200, "text/plain", "changed value"); 
  });

  // Not found handler
  server.onNotFound(handleNotFound);
}

void setupGPIO() {
  analogWriteFreq(500);
  analogWriteRange(255);
  
  pinMode(GPIO_POUT_R, OUTPUT);
  digitalWrite(GPIO_POUT_R, LOW);

  pinMode(GPIO_POUT_G, OUTPUT);
  digitalWrite(GPIO_POUT_G, LOW);

  pinMode(GPIO_POUT_B, OUTPUT);
  digitalWrite(GPIO_POUT_B, LOW);

  analogWrite(GPIO_POUT_R, 100);
  analogWrite(GPIO_POUT_G, 100);
  analogWrite(GPIO_POUT_B, 100);
}

// connect to UDP – returns true if successful or false if not
void connectUDP() {
    Serial.println("Connecting to UDP");
    udpConnected = Udp.begin(portMulti);
    Serial.println("Connected to UDP");
}

void setup(void) {
  setupGPIO();
  
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  WiFi.hostname("rgb-controller-"DEVICE_ID);

  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  connectUDP();

  Serial.print("Connected to WiFi");

  MDNS.begin(host);

  setupHTTPActions();

  httpUpdater.setup(&server);

  server.begin();

  MDNS.addService("http", "tcp", 80);
}

void handleUDP() {
  if(udpConnected) {
    int packetSize = Udp.parsePacket();
    if(packetSize) {
        // read the packet into packetBufffer
        Udp.read(udpPacketBuffer, UDP_TX_PACKET_MAX_SIZE);

        analogWrite(GPIO_POUT_R, udpPacketBuffer[0]);
        analogWrite(GPIO_POUT_G, udpPacketBuffer[1]);
        analogWrite(GPIO_POUT_B, udpPacketBuffer[2]);
    }
  }  
}

// ============= Main Loop =============
void loop(void) {
  server.handleClient();
  handleUDP();
}
