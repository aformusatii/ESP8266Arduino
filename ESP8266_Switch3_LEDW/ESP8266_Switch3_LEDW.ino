#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUDP.h>

#define GPIO_POUT_A 5

const char* ssid = "DEFENDOR";
const char* password = "PIC12F675TDA9811";

volatile bool udpConnected = false;
volatile bool out_port_status = false;

char udpPacketBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

// Multicast declarations
IPAddress ipMulti(239, 0, 0, 1);
unsigned int portMulti = 6000;      // local port to listen on

ESP8266WebServer server(80);

void indexPage() {
  String message = "<!doctype html>";
  message += "<html lang=\"en\">";
  message += "<head>";
  message += "<title>ESP8266 Switch 3 Controller</title>";
  message += "</head>";

  message += "<body>";
  message += "<a href=\"/switchOn\">switchOn</a><br/>";
  message += "<a href=\"/switchOff\">switchOff</a><br/>";
  message += "<a href=\"/switchToggle\">switchToggle</a><br/>";
  message += "<a href=\"/setLevel\">setLevel</a><br/>";
  message += "<a href=\"/status\">status</a><br/>";
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

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

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
