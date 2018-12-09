#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUDP.h>
#include <ESP8266HTTPClient.h>
#include <WiFiConfig.h>

#define DEVICE_ID      "2"

#define GPIO_POUT      12

const char* host = "switch-mod-"DEVICE_ID;

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

// UDP local port to listen on
unsigned int portMulti = 6001;

volatile bool out_port_status = false;

volatile bool udpConnected = false;
char udpPacketBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,

void indexPage() {
  String message = "<!doctype html>";
  message += "<html lang=\"en\">";
  message += "<head>";
  message += "<title>ESP8266 Switch Mod "DEVICE_ID"</title>";
  message += "</head>";
  message += "<body>";
  
  message += "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

  message += "--------------------------------------------------------------<br/>";

  message += "<a href=\"/switchOn\">switchOn</a><br/>";
  message += "<a href=\"/switchOff\">switchOff</a><br/>";
  message += "<a href=\"/switchToggle\">switchToggle</a><br/>";
  message += "<a href=\"/status\">status</a><br/>";

  uint32_t realSize = ESP.getFlashChipRealSize();
  uint32_t ideSize = ESP.getFlashChipSize();
  uint32_t chipId = ESP.getFlashChipId();
  uint32_t chipSpeed = ESP.getFlashChipSpeed();
  FlashMode_t ideMode = ESP.getFlashChipMode();

  char buf[255];

  message += "--------------------------------------------------------------<br/>";

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

  server.on("/switchOn", [](){
    server.send(200, "text/plain", "switch on");
    setPortState(true);
  });

  server.on("/switchOff", [](){
    server.send(200, "text/plain", "switch off");
    setPortState(false);
  });

  server.on("/switchToggle", [](){
    setPortState(!out_port_status);
    if (out_port_status) {
      server.send(200, "text/plain", "switch on"); 
    } else {
      server.send(200, "text/plain", "switch off"); 
    }
  });

  server.on("/status", [](){
    if (out_port_status) {
      server.send(200, "text/plain", "switch on"); 
    } else {
      server.send(200, "text/plain", "switch off"); 
    }
  });

  // Not found handler
  server.onNotFound(handleNotFound);
}

void setPortState(bool state) {
  out_port_status = state;

  if (state) {
    digitalWrite(GPIO_POUT, HIGH);
  } else {
    digitalWrite(GPIO_POUT, LOW);
  }
}

void setupGPIO() {  
  pinMode(GPIO_POUT, OUTPUT);
  digitalWrite(GPIO_POUT, LOW);
}

// connect to UDP – returns true if successful or false if not
void connectUDP() {
    Serial.println("Connecting to UDP");
    
    udpConnected = Udp.begin(portMulti);
    Serial.println("Connected to UDP");
}

void setup(void) {
  setupGPIO();

  setPortState(false);
  
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  WiFi.hostname(host);

  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  connectUDP();

  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

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
    }
  }
}

// ============= Main Loop =============
void loop(void) {
  server.handleClient();
  handleUDP();
}
