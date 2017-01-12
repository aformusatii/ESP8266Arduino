#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#define GPIO_POUT_A     14
#define GPIO_POUT_B     12
#define GPIO_POUT_C     16

volatile bool out_port_a_status = false;
volatile bool out_port_b_status = false;
volatile bool out_port_c_status = false;

const char* host = "socket1";
const char* ssid = "DEFENDOR";
const char* password = "****";

ESP8266WebServer server(80);

void indexPage() {
  String message = "<!doctype html>";
  message += "<html lang=\"en\">";
  message += "<head>";
  message += "<title>ESP8266 Socket 1</title>";
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
  
  message += "<a href=\"/switchOnA\">SwitchOn A</a><br/>";
  message += "<a href=\"/switchOffA\">SwitchOff A</a><br/>";
  message += "<a href=\"/switchToggleA\">SwitchToggle A</a><br/>";
  message += "<a href=\"/statusA\">Status A</a><br/>";

  message += "<a href=\"/switchOnB\">SwitchOn B</a><br/>";
  message += "<a href=\"/switchOffB\">SwitchOff B</a><br/>";
  message += "<a href=\"/switchToggleB\">SwitchToggle B</a><br/>";
  message += "<a href=\"/statusB\">Status B</a><br/>";

  message += "<a href=\"/switchOnC\">SwitchOn C</a><br/>";
  message += "<a href=\"/switchOffC\">SwitchOff C</a><br/>";
  message += "<a href=\"/switchToggleC\">SwitchToggle C</a><br/>";
  message += "<a href=\"/statusC\">Status C</a><br/>";
  
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
    setPortStateA(true);
  });

  server.on("/switchOffA", [](){
    server.send(200, "text/plain", "switch off");
    setPortStateA(false);
  });

  server.on("/switchToggleA", [](){
    setPortStateA(!out_port_a_status);
    if (out_port_a_status) {
      server.send(200, "text/plain", "switch on"); 
    } else {
      server.send(200, "text/plain", "switch off"); 
    }
  });

  server.on("/statusA", [](){
    if (out_port_a_status) {
      server.send(200, "text/plain", "switch on"); 
    } else {
      server.send(200, "text/plain", "switch off"); 
    }
  });

  // ============= Port B =============
  server.on("/switchOnB", [](){
    server.send(200, "text/plain", "switch on");
    setPortStateB(true);
  });

  server.on("/switchOffB", [](){
    server.send(200, "text/plain", "switch off");
    setPortStateB(false);
  });

  server.on("/switchToggleB", [](){
    setPortStateB(!out_port_b_status);
    if (out_port_b_status) {
      server.send(200, "text/plain", "switch on"); 
    } else {
      server.send(200, "text/plain", "switch off"); 
    }
  });

  server.on("/statusB", [](){
    if (out_port_b_status) {
      server.send(200, "text/plain", "switch on"); 
    } else {
      server.send(200, "text/plain", "switch off"); 
    }
  });

  // ============= Port C =============
  server.on("/switchOnC", [](){
    server.send(200, "text/plain", "switch on");
    setPortStateC(true);
  });

  server.on("/switchOffC", [](){
    server.send(200, "text/plain", "switch off");
    setPortStateC(false);
  });

  server.on("/switchToggleC", [](){
    setPortStateC(!out_port_c_status);
    if (out_port_c_status) {
      server.send(200, "text/plain", "switch on"); 
    } else {
      server.send(200, "text/plain", "switch off"); 
    }
  });

  server.on("/statusC", [](){
    if (out_port_c_status) {
      server.send(200, "text/plain", "switch on"); 
    } else {
      server.send(200, "text/plain", "switch off"); 
    }
  });

  // Not found handler
  server.onNotFound(handleNotFound);
}

void setupGPIO() {
  pinMode(GPIO_POUT_A, OUTPUT);
  digitalWrite(GPIO_POUT_A, LOW);

  pinMode(GPIO_POUT_B, OUTPUT);
  digitalWrite(GPIO_POUT_B, LOW);

  pinMode(GPIO_POUT_C, OUTPUT);
  digitalWrite(GPIO_POUT_C, LOW);
}

void setup(void) {
  setupGPIO();
  
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

  MDNS.begin(host);

  setupHTTPActions();

  server.begin();

  MDNS.addService("http", "tcp", 80);
  
  Serial.println("HTTP server started");
  Serial.println("v0.3");
}

void setPortStateA(bool state) {
  out_port_a_status = state;

  if (state) {
    digitalWrite(GPIO_POUT_A, HIGH);
  } else {
    digitalWrite(GPIO_POUT_A, LOW);
  }
}

void setPortStateB(bool state) {
  out_port_b_status = state;

  if (state) {
    digitalWrite(GPIO_POUT_B, HIGH);
  } else {
    digitalWrite(GPIO_POUT_B, LOW);
  }
}

void setPortStateC(bool state) {
  out_port_c_status = state;

  if (state) {
    digitalWrite(GPIO_POUT_C, HIGH);
  } else {
    digitalWrite(GPIO_POUT_C, LOW);
  }
}

// ============= Main Loop =============
void loop(void) {
  server.handleClient();  
}
