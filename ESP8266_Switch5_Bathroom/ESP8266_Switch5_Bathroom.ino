#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUDP.h>
#include <WiFiConfig.h>

#define GPIO_PIN_A     14
#define GPIO_POUT_A    5
#define GPIO_POUT_B    4

const char* host = "switch5";

volatile unsigned long wifi_check_status_millis = 0;
volatile unsigned long wifi_check_rssi_millis = 0;

volatile unsigned long trigger_at_a_millis = 0;
volatile unsigned long trigger_at_b_millis = 0;

volatile bool port_a_trigger = false;
volatile bool port_b_trigger = false;

volatile bool out_port_a_status = false;
volatile bool out_port_b_status = false;

volatile int port_a_value = LOW;

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
  message += "<title>ESP8266 Switch 5 Controller (Bathroom)</title>";
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
  server.on("/statusB", [](){
    if (out_port_b_status) {
      server.send(200, "text/plain", "switch on"); 
    } else {
      server.send(200, "text/plain", "switch off"); 
    }
  });

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

  // Not found handler
  server.onNotFound(handleNotFound);
}

void setupGPIO() {
  pinMode(GPIO_PIN_A, INPUT);
  pinMode(GPIO_POUT_A, OUTPUT);
  digitalWrite(GPIO_POUT_A, LOW);
  
  pinMode(GPIO_POUT_B, OUTPUT);
  digitalWrite(GPIO_POUT_B, LOW);

  delay(200);

  port_a_value = digitalRead(GPIO_PIN_A);
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

  httpUpdater.setup(&server);

  server.begin();

  MDNS.addService("http", "tcp", 80);
  
  Serial.println("HTTP server started");
  Serial.println("v0.1");
}

// ============ Port A ============
void changeCallbackA() {
  setPortStateA(!out_port_a_status);    
}

void setPortStateA(bool state) {
  out_port_a_status = state;

  Udp.beginPacketMulticast(ipMulti, portMulti, WiFi.localIP());

  if (state) {
    digitalWrite(GPIO_POUT_A, HIGH);
    Serial.println("HIGH A");
    Udp.write("SWITCH5->A ON");
  } else {
    digitalWrite(GPIO_POUT_A, LOW);
    Serial.println("LOW A");
    Udp.write("SWITCH5->A OFF");
  }

  Udp.endPacket();
}

// ============ Port B ============
void setPortStateB(bool state) {
  out_port_b_status = state;

  Udp.beginPacketMulticast(ipMulti, portMulti, WiFi.localIP());

  if (state) {
    digitalWrite(GPIO_POUT_B, HIGH);
    Serial.println("HIGH B");
    Udp.write("SWITCH5->B ON");
  } else {
    digitalWrite(GPIO_POUT_B, LOW);
    Serial.println("LOW B");
    Udp.write("SWITCH5->B OFF");
  }

  Udp.endPacket();
}

void handlePortA() {
  long currentTime = millis();
  
  if (port_a_trigger && (trigger_at_a_millis < currentTime)) {
    port_a_trigger = false;
    changeCallbackA();
  }

  int a1 = digitalRead(GPIO_PIN_A);
  if (port_a_value != a1) {
    trigger_at_a_millis = millis() + 200;
    port_a_trigger = true;
  }

  port_a_value = a1;  
}

// ============= Main Loop =============
void loop(void) {
  server.handleClient();
  
  handlePortA();
}
