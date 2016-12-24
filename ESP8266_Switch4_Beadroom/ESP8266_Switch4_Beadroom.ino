#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUDP.h>

#define GPIO_PIN_A_1   5
#define GPIO_PIN_A_2   14
#define GPIO_PIN_A_3   10
#define GPIO_POUT_A    13

#define GPIO_PIN_B_1   4
#define GPIO_POUT_B    12

const char* ssid = "DEFENDOR";
const char* password = "PIC12F675TDA9811";

volatile unsigned long trigger_at_a_millis = 0;
volatile unsigned long trigger_at_b_millis = 0;

volatile bool port_a_trigger = false;
volatile bool port_b_trigger = false;

volatile bool out_port_a_status = false;
volatile bool out_port_b_status = false;

volatile int port_a_1_value = LOW;
volatile int port_a_2_value = LOW;
volatile int port_a_3_value = LOW;
volatile int port_b_1_value = LOW;

boolean udpConnected = false;
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
  message += "<title>ESP8266 Switch 4 Controller</title>";
  message += "</head>";

  message += "<body>";
  
  message += "<a href=\"/switchOnA\">SwitchOn A</a><br/>";
  message += "<a href=\"/switchOffA\">SwitchOff A</a><br/>";
  message += "<a href=\"/switchToggleA\">SwitchToggle A</a><br/>";
  message += "<a href=\"/statusA\">Status A</a><br/>";

  message += "<a href=\"/switchOnB\">SwitchOn B</a><br/>";
  message += "<a href=\"/switchOffB\">SwitchOff B</a><br/>";
  message += "<a href=\"/switchToggleB\">SwitchToggle B</a><br/>";
  message += "<a href=\"/statusB\">Status B</a><br/>";
  
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
  pinMode(GPIO_PIN_A_1, INPUT);
  pinMode(GPIO_PIN_A_2, INPUT);
  pinMode(GPIO_PIN_A_3, INPUT);
  pinMode(GPIO_POUT_A, OUTPUT);

  pinMode(GPIO_PIN_B_1, INPUT);
  pinMode(GPIO_POUT_B, OUTPUT);

  delay(200);

  /* attachInterrupt(GPIO_PIN_A_1, changeAInterrupt, CHANGE);
  attachInterrupt(GPIO_PIN_A_2, changeAInterrupt, CHANGE);
  attachInterrupt(GPIO_PIN_A_3, changeAInterrupt, CHANGE);
  
  attachInterrupt(GPIO_PIN_B_1, changeBInterrupt, CHANGE); */

  port_a_1_value = digitalRead(GPIO_PIN_A_1);
  port_a_2_value = digitalRead(GPIO_PIN_A_2);
  port_a_3_value = digitalRead(GPIO_PIN_A_3);
  port_b_1_value = digitalRead(GPIO_PIN_B_1);
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

  udpConnected = connectUDP();

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  setupHTTPActions();

  server.begin();
  Serial.println("HTTP server started");

  setupGPIO();
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

// ============ Port A ============
void changeAInterrupt() {
  trigger_at_a_millis = millis() + 200;
  port_a_trigger = true;
}

void changeCallbackA() {
  setPortStateA(!out_port_a_status);    
}

void setPortStateA(bool state) {
  out_port_a_status = state;

  Udp.beginPacketMulticast(ipMulti, portMulti, WiFi.localIP());

  if (state) {
    digitalWrite(GPIO_POUT_A, HIGH);
    Serial.println("HIGH A");
    Udp.write("SWITCH4->A ON");
  } else {
    digitalWrite(GPIO_POUT_A, LOW);
    Serial.println("LOW A");
    Udp.write("SWITCH4->A OFF");
  }

  Udp.endPacket();
}

// ============ Port B ============
void changeBInterrupt() {
  trigger_at_b_millis = millis() + 200;
  port_b_trigger = true;
}

void changeCallbackB() {
  setPortStateB(!out_port_b_status);    
}

void setPortStateB(bool state) {
  out_port_b_status = state;

  Udp.beginPacketMulticast(ipMulti, portMulti, WiFi.localIP());

  if (state) {
    digitalWrite(GPIO_POUT_B, HIGH);
    Serial.println("HIGH B");
    Udp.write("SWITCH4->B ON");
  } else {
    digitalWrite(GPIO_POUT_B, LOW);
    Serial.println("LOW B");
    Udp.write("SWITCH4->B OFF");
  }

  Udp.endPacket();
}

// ============= Main Loop =============
void loop(void) {
  server.handleClient();
  
  if (port_a_trigger && (trigger_at_a_millis < millis())) {
    port_a_trigger = false;
    changeCallbackA();
  }

  if (port_b_trigger && (trigger_at_b_millis < millis())) {
    port_b_trigger = false;
    changeCallbackB();
  }

  int a1 = digitalRead(GPIO_PIN_A_1);
  int a2 = digitalRead(GPIO_PIN_A_2);
  int a3 = digitalRead(GPIO_PIN_A_3);

  if ( (port_a_1_value != a1) || (port_a_2_value != a2) || port_a_3_value != a3) {
    changeAInterrupt();
  }
  
  port_a_1_value = a1;
  port_a_2_value = a2;
  port_a_3_value = a3;

  int b1 = digitalRead(GPIO_PIN_B_1);

  if (port_b_1_value != b1) {
    changeBInterrupt();
  }

  port_b_1_value = b1;

  /* if (digitalRead(GPIO_PIN_B_1) == HIGH) {
    Serial.println("GPIO_PIN_B_1 HIGH");
  }*/
  
}
