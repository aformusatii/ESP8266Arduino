#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUDP.h>

#define GPIO_PIN_A 10
#define GPIO_POUT_A 4

#define GPIO_PIN_B 5

const char* ssid = "DEFENDOR";
const char* password = "****";

volatile unsigned long trigger_at_a_millis = 0;
volatile unsigned long trigger_at_b_millis = 0;

volatile bool port_a_trigger = false;
volatile bool port_b_trigger = false;

volatile bool out_port_status = false;

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
  message += "<title>ESP8266 Switch 1 Controller</title>";
  message += "</head>";

  message += "<body>";
  message += "<a href=\"/switchOn\">switchOn</a><br/>";
  message += "<a href=\"/switchOff\">switchOff</a><br/>";
  message += "<a href=\"/switchToggle\">switchToggle</a><br/>";
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

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  pinMode(GPIO_PIN_A, INPUT_PULLUP);
  pinMode(GPIO_PIN_B, INPUT_PULLUP);
  pinMode(GPIO_POUT_A, OUTPUT);

  attachInterrupt(GPIO_PIN_A, changeAInterrupt, CHANGE);
  attachInterrupt(GPIO_PIN_B, changeBInterrupt, CHANGE);
}

void changeAInterrupt() {
  trigger_at_a_millis = millis() + 200;
  port_a_trigger = true;
}

void changeBInterrupt() {
  trigger_at_b_millis = millis() + 200;
  port_b_trigger = true;
}

void changeCallback() {
  setPortState(!out_port_status);    
}

void setPortState(bool state) {
  out_port_status = state;

  Udp.beginPacketMulticast(ipMulti, portMulti, WiFi.localIP());

  if (state) {
    digitalWrite(GPIO_POUT_A, HIGH);
    Serial.println("HIGH");
    Udp.write("SWITCH1->A ON");
  } else {
    digitalWrite(GPIO_POUT_A, LOW);
    Serial.println("LOW");
    Udp.write("SWITCH1->A OFF");
  }

  Udp.endPacket();
}

void loop(void){
  server.handleClient();
  
  if (port_a_trigger && (trigger_at_a_millis < millis())) {
    port_a_trigger = false;
    changeCallback();
  }

  if (port_b_trigger && (trigger_at_b_millis < millis())) {
    port_b_trigger = false;
    Serial.println("B");
    
    Udp.beginPacketMulticast(ipMulti, portMulti, WiFi.localIP());
    Udp.write("SWITCH1->B TOGGLE");
    Udp.endPacket();  
  }
  
}
