#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUDP.h>
#include <NDIRZ16.h>
#include <SC16IS750.h>
#include <ESP8266HTTPClient.h>
#include <WiFiConfig.h>

#define SENSOR_READ_AND_SEND_INTERVAL_MIN  15
#define SENSOR_INITIALIZATION_INTERVAL_SEC  200

const char* host = "MHZ16-1";

volatile unsigned long timer_sensor_initialization = 0;
volatile unsigned long timer_sensor_read_and_send = 0;
volatile unsigned long timer_sensor_interim_reads = 0;

IPAddress udpRemoteIP(192, 168, 1, 116);
unsigned int udpRemotePort = 6002; 

WiFiUDP Udp;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

SC16IS750 i2cuart = SC16IS750(SC16IS750_PROTOCOL_I2C,SC16IS750_ADDRESS_BB);
NDIRZ16 mySensor = NDIRZ16(&i2cuart);

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

  message += "<a href=\"/switchOn\">SwitchOn Sensor</a><br/>";
  message += "<a href=\"/switchOff\">SwitchOff Sensor</a><br/>";
  
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
    timer_sensor_read_and_send = millis();
  });

  server.on("/switchOff", [](){
    server.send(200, "text/plain", "switch off");
  });

  // Not found handler
  server.onNotFound(handleNotFound);
}

void setupGPIO() {
}

void setup(void) {
  setupGPIO();
  
  Serial.begin(9600);
  i2cuart.begin(9600);

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

  //timer_counter_1 = millis() + 15000;

  if (i2cuart.ping()) {
      sendData("SC16IS750 found.");
      timer_sensor_read_and_send = millis();
  } else {
      sendData("SC16IS750 not found.");
  }
}

// ============= Main Loop =============
void loop(void) {
  server.handleClient();

  unsigned long currentTime = millis();

  if ( (timer_sensor_read_and_send > 0) && (timer_sensor_read_and_send < currentTime) ) {
    sendData("Read and send sensor data\n");
    timer_sensor_read_and_send = currentTime + ((SENSOR_READ_AND_SEND_INTERVAL_MIN * 60) * 1000);
    timer_sensor_initialization = currentTime + (SENSOR_INITIALIZATION_INTERVAL_SEC * 1000);
    timer_sensor_interim_reads = currentTime + 10000;
    power(HIGH);
  }

  if ( (timer_sensor_interim_reads > 0) && (timer_sensor_interim_reads < currentTime) ) {
    timer_sensor_interim_reads = currentTime + 10000;

    if (mySensor.measure()) {
        char buffer[255];
        sprintf(buffer, "INTERIM: ppm=[%d] t=[%d]\n", mySensor.ppm, mySensor.temperature);
        sendData(buffer);
    } else {
        sendData("Failed to read sensor data\n");  
    }
  }

  if ( (timer_sensor_initialization > 0) && (timer_sensor_initialization < currentTime) ) {
    sendData("Sensor initialized\n");
    timer_sensor_initialization = 0; //trigger read just once
    timer_sensor_interim_reads = 0;
    
    if (mySensor.measure()) {
        //char buffer[255];
        //sprintf(buffer, "ppm=[%d] t=[%d]\n", mySensor.ppm, mySensor.temperature);
        //sendData(buffer);
        sendSensorDataOvertHttp(mySensor.ppm, mySensor.temperature);
    } else {
        sendData("Failed to read sensor data\n");  
    }

    power(LOW);
  }
  
}

//Power control function for NDIR sensor. 1=ON, 0=OFF
void power (uint8_t state) {
    if (state) {
        i2cuart.pinMode(0, INPUT);  //turn on the power of MH-Z16
    } else {
        i2cuart.pinMode(0, OUTPUT);
        i2cuart.digitalWrite(0, LOW); //turn off the power of MH-Z16
    }
}

//Zero Point (400ppm) Calibration function for NDIR sensor. Only used when necessary.
//Make sure that the sensor has been running in fresh air (~400ppm) for at least 20 minutes before calling this function.
void calibrate() {
    i2cuart.pinMode(1, OUTPUT);    //set up for the calibration pin.

    i2cuart.digitalWrite(1, LOW);  //start calibration of MH-Z16 under 400ppm
    delay(10000);                  //5+ seconds needed for the calibration process
    i2cuart.digitalWrite(1, HIGH); //toggle the pin HIGH back to normal operation
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

void sendSensorDataOvertHttp(uint32_t ppm, uint8_t  temperature) {
    char url[255];
    sprintf(url, "http://192.168.1.116/misc/save_sensor_data_http.php?sensor=3&type=4&value=%d", ppm);
    
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    
    if(httpCode > 0) {
        sendData("Data sent\n");
    } else {
        sendData("Failed to send sensor data over http\n");  
    }

    http.end();
}

