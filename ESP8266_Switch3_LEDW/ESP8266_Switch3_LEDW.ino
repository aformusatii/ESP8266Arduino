#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUDP.h>
#include <WiFiConfig.h>
#include <ESP8266HTTPClient.h>

#define GPIO_POUT_A 5

volatile bool udpConnected = false;
volatile bool out_port_status = false;

char udpPacketBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,

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
  message += "<title>ESP8266 Switch 3 Controller v0.2</title>";
  message += "</head>";

  message += "<body>";
  
  message += "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  message += "--------------------------------------------------------------<br/>";
  
  message += "<a href=\"/switchOn\">switchOn</a><br/>";
  message += "<a href=\"/switchOff\">switchOff</a><br/>";
  message += "<a href=\"/switchToggle\">switchToggle</a><br/>";
  message += "<a href=\"/setLevel\">setLevel</a><br/>";
  message += "<a href=\"/status\">status</a><br/>";
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
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  udpConnected = connectUDP();

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

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

  server.on("/setLevel", [](){
    String valueStr = server.arg("value");
    int value = valueStr.toInt();
    analogWrite(GPIO_POUT_A, value);
    out_port_status = value > 0;
    Serial.println(value);
    server.send(200, "text/plain", "change level"); 
  });

  server.on("/setSpeed", [](){
    String url = server.arg("url");

    WiFiClient client;
    HTTPClient http;
    http.begin(client, url);

    int httpCode = http.GET();

    String message = "";
    
    if (httpCode == HTTP_CODE_OK) {
        // get lenght of document (is -1 when Server sends no Content-Length header)
        int len = http.getSize();
        int total_len = http.getSize();

        // create buffer for read
        uint8_t buff[2000] = { 0 };

        // get tcp stream
        WiFiClient * stream = &client;

        unsigned long start = millis();

        // read all data from server
        while (http.connected() && (len > 0 || len == -1)) {
          // get available data size
          size_t size = stream->available();

          if (size) {
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

            if (len > 0) {
              len -= c;
            }
          }
        }

        unsigned long elapsed = millis() - start;

        message += "{\n";
        message += "\"totalBytes\": ";
        message += total_len;
        message += ",\n";
        message += "\"totalElapsed\": ";
        message += elapsed; 
        message += ",\n";
        message += "\"bytesPerSecond\": ";
        message += ( (total_len * 1000) / elapsed );
        message += "\n";
        message += "}";
    } else {
        message += "{\n";
        message += "\"httpCode\": ";
        message += httpCode;
        message += "}";
    }

    http.end();
    
    server.send(200, "application/json", message); 
  });

  server.onNotFound(handleNotFound);

  httpUpdater.setup(&server);

  server.begin();
  
  Serial.println("HTTP server started");

  analogWriteFreq(50000);
  pinMode(GPIO_POUT_A, OUTPUT);
}

// connect to UDP – returns true if successful or false if not
boolean connectUDP() {
    boolean state = false;

    Serial.println("");
    Serial.println("Connecting to UDP");

    if(Udp.beginMulticast(WiFi.localIP(), ipMulti, portMulti) == 1) {
        Serial.println("Connection successful");
        state = true;
    } else {
        Serial.println("Connection failed");
    }

    return state;
}

void setPortState(bool state) {
  out_port_status = state;

  analogWrite(GPIO_POUT_A, 0);

  if (state) {
    digitalWrite(GPIO_POUT_A, HIGH);
    Serial.println("HIGH");
  } else {
    digitalWrite(GPIO_POUT_A, LOW);
    Serial.println("LOW");
  }
}

void handleUDP() {
  if(udpConnected) {
    int packetSize = Udp.parsePacket();
    if(packetSize) {
        // read the packet into packetBufffer
        Udp.read(udpPacketBuffer, UDP_TX_PACKET_MAX_SIZE);
        
        if (packetSize < UDP_TX_PACKET_MAX_SIZE) {
          udpPacketBuffer[packetSize] = 0;
        }

        if (strcmp(udpPacketBuffer, "SWITCH1->A ON") == 0) {
          Serial.println("FROM: SWITCH1->A ON");
          setPortState(true);
        } else if (strcmp(udpPacketBuffer, "SWITCH1->A OFF") == 0) {
          Serial.println("FROM: SWITCH1->A OFF");
          setPortState(false);
        }
    }
  }  
}

void loop(void){
  server.handleClient();
  handleUDP();
}
