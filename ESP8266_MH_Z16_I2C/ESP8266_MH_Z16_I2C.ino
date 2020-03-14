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
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#include <FS.h>

// ================================================= MQTT Constants =================================================
#define MQTT_SERVER      "192.168.1.116"
#define MQTT_PORT        1883                   // use 8883 for SSL
#define MQTT_USERNAME    "TEST"
#define MQTT_KEY         "TEST"

// ================================================= Misc Constants =================================================
#define SENSOR_READ_AND_SEND_INTERVAL_MIN         15
#define SENSOR_INITIALIZATION_INTERVAL_SEC       200
#define SENSOR_INTERIM_READ_INTERVAL_SEC          10

#define DUST_SENSOR_READ_AND_SEND_INTERVAL_MIN    15
#define DUST_SENSOR_INITIALIZATION_INTERVAL_SEC   30
#define DUST_SENSOR_INTERIM_READ_INTERVAL_SEC      4
#define DUST_SENSOR_AVG_INTERVAL_SEC              60

#define RAD_SENSOR_READ_AND_SEND_INTERVAL_MIN      5
#define RAD_SENSOR_AVG_INTERVAL_SEC               60

#define CO2_SENSOR_ID                              "AF_ML_00101"
#define DUST_SENSOR_ID                             "AF_ML_00102"
#define RAD_SENSOR_ID                              "AF_ML_00103"

#define SENSOR_PPM_TYPE                            "co2"
#define SENSOR_PM1_0_TYPE                          "pm1_0"
#define SENSOR_PM2_5_TYPE                          "pm2_5"
#define SENSOR_PM10_0_TYPE                         "pm10_0"
#define SENSOR_RCPM_TYPE                           "rad"

#define PLANTOWER_SET_PIN  16
#define PLANTOWER_RST_PIN   5
#define RADIATION_INT_PIN   4

// ================================================= Variables =================================================

const char* host = "MHZ16-1";

volatile unsigned long timer_CO2_sensor_initialization = 0;
volatile unsigned long timer_CO2_sensor_start = 0;
volatile unsigned long timer_CO2_sensor_interim_reads = 0;

volatile unsigned long timer_dust_sensor_start = 0;
volatile unsigned long timer_dust_sensor_interim_reads = 0;
volatile unsigned long timer_dust_sensor_send = 0;

volatile unsigned long timer_rad_sensor_start = 0;
volatile unsigned long timer_rad_sensor_send = 0;

volatile unsigned long dust_total_pm_1_0_value = 0;
volatile unsigned long dust_total_pm_2_5_value = 0;
volatile unsigned long dust_total_pm_10_0_value = 0;

volatile int dust_read_count = 0;

volatile int radiation_count = 0;

// !!! Command to listen: netcat -ul 6002
IPAddress udpRemoteIP(192, 168, 1, 116);
unsigned int udpRemotePort = 6002; 

WiFiUDP Udp;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
File fsUploadFile;

uint16_t calcChecksum = 0;

int incomingByte = 0; // for incoming serial data
const int MAX_FRAME_LEN = 64;
char frameBuf[MAX_FRAME_LEN];
int detectOff = 0;
int frameLen = MAX_FRAME_LEN;
bool inFrame = false;
char printbuf[256];
const bool DEBUG = false;

volatile uint32_t co2_ppm = 0;
volatile uint32_t pm_1_0_value = 0;
volatile uint32_t pm_2_5_value = 0;
volatile uint32_t pm_10_0_value = 0;
volatile uint32_t radiation_value = 0;

struct PMS7003_framestruct {
    uint8_t  frameHeader[2];
    uint16_t frameLen = MAX_FRAME_LEN;
    uint16_t concPM1_0_CF1;
    uint16_t concPM2_5_CF1;
    uint16_t concPM10_0_CF1;
    uint16_t concPM1_0_amb;
    uint16_t concPM2_5_amb;
    uint16_t concPM10_0_amb;
    uint16_t rawGt0_3um;
    uint16_t rawGt0_5um;
    uint16_t rawGt1_0um;
    uint16_t rawGt2_5um;
    uint16_t rawGt5_0um;
    uint16_t rawGt10_0um;
    uint8_t  version;
    uint8_t  errorCode;
    uint16_t checksum;
} thisFrame;

SC16IS750 i2cuart = SC16IS750(SC16IS750_PROTOCOL_I2C,SC16IS750_ADDRESS_BB);
NDIRZ16 mySensor = NDIRZ16(&i2cuart);

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_PORT);

Adafruit_MQTT_Publish sensorPublisher = Adafruit_MQTT_Publish(&mqtt, "/sensor/common/data");

// ================================================= FSBrowser =================================================

//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
  }
}

void handleFileDelete(){
  if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate(){
  if(server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if(file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if(!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}
  
  String path = server.arg("dir");
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  
  output += "]";
  server.send(200, "text/json", output);
}


// ================================================= Functions =================================================

void indexPage() {
  String message = "<!doctype html>";
  message += "<html lang=\"en\">";
  message += "<head>";
  message += "<title>ESP8266 MH-Z16 Sensor v0.2</title>";
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

  message += "BSSID MAC: ";
  message.concat(WiFi.BSSIDstr());
  message += "<br/>";

  uint32_t realSize = ESP.getFlashChipRealSize();
  uint32_t ideSize = ESP.getFlashChipSize();
  uint32_t chipId = ESP.getFlashChipId();
  uint32_t chipSpeed = ESP.getFlashChipSpeed();
  FlashMode_t ideMode = ESP.getFlashChipMode();

  char buf[255];

  sprintf(buf, "Flash chip id: %08X<br/>Flash chip speed: %u<br/>Flash chip size: %u<br/>Flash ide mode: %s<br/>Flash real size: %u",
    chipId, 
    chipSpeed,
    ideSize,
    (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"),
    realSize);
    
  message += buf;
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
    timer_CO2_sensor_start = millis();
  });

  server.on("/switchOff", [](){
    server.send(200, "text/plain", "switch off");
  });

  server.on("/sensor/1", [](){
    char buf[255];
    String callback = server.arg("callback");

    if (callback.length() > 0) {
      sprintf(buf, "%s({\"value\":%u});",
        callback.c_str(),
        co2_ppm);
  
      server.send(200, "text/plain", buf);
    } else {
      sprintf(buf, "{\"value\":%u}", co2_ppm);
      server.send(200, "application/json", buf);
    }
  });

  server.on("/sensor/2", [](){
    char buf[255];
    String callback = server.arg("callback");

    if (callback.length() > 0) {
      sprintf(buf, "%s({\"pm_1_0\":%u, \"pm_2_5\":%u, \"pm_10_0\":%u});",
        callback.c_str(),
        pm_1_0_value,
        pm_2_5_value,
        pm_10_0_value);
  
      server.send(200, "text/plain", buf);
    } else {
      sprintf(buf, "{\"pm_1_0\":%u, \"pm_2_5\":%u, \"pm_10_0\":%u}",
        pm_1_0_value,
        pm_2_5_value,
        pm_10_0_value);
      server.send(200, "application/json", buf);      
    }

  });

  server.on("/sensor/3", [](){
    char buf[255];
    String callback = server.arg("callback");

    if (callback.length() > 0) {
      sprintf(buf, "%s({\"value\":%u});",
        callback.c_str(),
        radiation_value);
  
      server.send(200, "text/plain", buf);
    } else {
      sprintf(buf, "{\"value\":%u}", radiation_value);
      server.send(200, "application/json", buf);     
    }
  });

  server.on("/sensors", [](){
    char buf[255];
    String callback = server.arg("callback");

    sprintf(buf, "{\"rad\": %u, \"pm_1_0\": %u, \"pm_2_5\": %u, \"pm_10_0\": %u, \"co2\": %u}",
            radiation_value,
            pm_1_0_value,
            pm_2_5_value,
            pm_10_0_value,
            co2_ppm);
    
    server.send(200, "application/json", buf);     
  });

  // Not found handler
  server.onNotFound(handleNotFound);
}

void setupGPIO() {
  pinMode(PLANTOWER_SET_PIN, OUTPUT);
  digitalWrite(PLANTOWER_SET_PIN, HIGH);

  pinMode(PLANTOWER_RST_PIN, OUTPUT);
  digitalWrite(PLANTOWER_RST_PIN, HIGH);

  pinMode(RADIATION_INT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RADIATION_INT_PIN), incrementRadiationCount, RISING);
}

void setupFSBrowser() {
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  
  //load editor
  server.on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound");
  });
  
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, [](){
    String json = "{";
    json += "\"heap\":"+String(ESP.getFreeHeap());
    json += ", \"analog\":"+String(analogRead(A0));
    json += ", \"gpio\":"+String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });
}

void setup(void) {
  setupGPIO();
  
  Serial.begin(9600);
  i2cuart.begin(9600);

  SPIFFS.begin();

  // WiFi.softAP("multi-sensor");
  WiFi.mode(WIFI_STA);

  WiFi.hostname("multi-sensor");

  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  MDNS.begin(host);

  setupHTTPActions();

  // setupFSBrowser();

  httpUpdater.setup(&server);

  server.begin();

  MDNS.addService("http", "tcp", 80);

  if (i2cuart.ping()) {
      sendData("SC16IS750 found.");
      timer_CO2_sensor_start = millis();
  } else {
      sendData("SC16IS750 not found.");
  }

  // Init Dust Sensor Timer
  timer_dust_sensor_start = millis();

  // Init Radiation Sensor Timer
  timer_rad_sensor_start = millis();
}

// ============================================================================================================================
// =================================================== MAIN Loop ==============================================================
// ============================================================================================================================
void loop(void) {
  server.handleClient();

  // CO2 Sensor
  handleCO2Sensor();

  // Dust Sensor
  handleDustSensor();

  // Radiation Sensor
  handleRadiationSensor(); 

  // Ensure the connection to the MQTT server is alive (this will make the first connection and automatically reconnect when disconnected).
  // See the MQTT_connect function definition further below.
  MQTT_connect();
}

// ============================================================================================================================
// =================================================== CO2 Sensor Functions ===================================================
// ============================================================================================================================
void handleCO2Sensor() {
  unsigned long currentTime = millis();

  // Start read and send cycle
  if ( (timer_CO2_sensor_start > 0) && (timer_CO2_sensor_start < currentTime) ) {
    sendData("Start read and send CO2 Sensor Data\n");
    timer_CO2_sensor_start = currentTime + ((SENSOR_READ_AND_SEND_INTERVAL_MIN * 60) * 1000);
    timer_CO2_sensor_initialization = currentTime + (SENSOR_INITIALIZATION_INTERVAL_SEC * 1000);
    timer_CO2_sensor_interim_reads = currentTime + (SENSOR_INTERIM_READ_INTERVAL_SEC * 1000);
    powerCO2Sensor(HIGH);
  }

  // Interim read
  if ( (timer_CO2_sensor_interim_reads > 0) && (timer_CO2_sensor_interim_reads < currentTime) ) {
    timer_CO2_sensor_interim_reads = currentTime + (SENSOR_INTERIM_READ_INTERVAL_SEC * 1000);

    if (mySensor.measure()) {
        char buffer[255];
        sprintf(buffer, "INTERIM: ppm=[%d] t=[%d]\n", mySensor.ppm, mySensor.temperature);
        sendData(buffer);
        co2_ppm = mySensor.ppm;
    } else {
        sendData("Failed to read CO2 sensor data\n");  
    }
  }

  // Final read and send
  if ( (timer_CO2_sensor_initialization > 0) && (timer_CO2_sensor_initialization < currentTime) ) {
    timer_CO2_sensor_initialization = 0; //trigger final read just once
    timer_CO2_sensor_interim_reads = 0; //trigger interim reads just once

    sendData("CO2 sensor initialized\n");
    
    if (mySensor.measure()) {
        co2_ppm = mySensor.ppm;
        sendSensorData(CO2_SENSOR_ID, SENSOR_PPM_TYPE, co2_ppm);
    } else {
        sendData("Failed to read CO2 sensor data\n");  
    }

    powerCO2Sensor(LOW);
  }
}

//Power control function for NDIR sensor. 1=ON, 0=OFF
void powerCO2Sensor (uint8_t state) {
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

// ============================================================================================================================
// =================================================== Dust Sensor Functions ==================================================
// ============================================================================================================================
void handleDustSensor() {
    unsigned long currentTime = millis();

    // Start read and send cycle
    if ( (timer_dust_sensor_start > 0) && (timer_dust_sensor_start < currentTime) ) {
      sendData("Start read and send dust sensor data\n");

      timer_dust_sensor_start = currentTime + ((DUST_SENSOR_READ_AND_SEND_INTERVAL_MIN * 60) * 1000);
      timer_dust_sensor_interim_reads = currentTime + (DUST_SENSOR_INITIALIZATION_INTERVAL_SEC * 1000);
      timer_dust_sensor_send = currentTime + (DUST_SENSOR_INITIALIZATION_INTERVAL_SEC * 1000) + (DUST_SENSOR_AVG_INTERVAL_SEC * 1000);

      dust_total_pm_1_0_value = 0;
      dust_total_pm_2_5_value = 0;
      dust_total_pm_10_0_value = 0;
      dust_read_count = 0;
      
      powerDustSensor(HIGH);
    }
    
    // Interim read (averaging)
    if ( (timer_dust_sensor_interim_reads > 0) && (timer_dust_sensor_interim_reads < currentTime) ) {
      timer_dust_sensor_interim_reads = currentTime + (DUST_SENSOR_INTERIM_READ_INTERVAL_SEC * 1000);
    
      if (pms7003_read()) {
          char buffer[255];
          sprintf(buffer, "PM1.0=%d, PM2.5=%d, PM10.0=%d \n", thisFrame.concPM1_0_amb, thisFrame.concPM2_5_amb, thisFrame.concPM10_0_amb);
          sendData(buffer);

          dust_total_pm_1_0_value += thisFrame.concPM1_0_amb;
          dust_total_pm_2_5_value += thisFrame.concPM2_5_amb;
          dust_total_pm_10_0_value += thisFrame.concPM10_0_amb;
          dust_read_count++;
      } else {
          sendData("Failed to read dust sensor data\n");  
      }
    }
    
    // Final read and send
    if ( (timer_dust_sensor_send > 0) && (timer_dust_sensor_send < currentTime) ) {
      timer_dust_sensor_send = 0; //trigger final read just once
      timer_dust_sensor_interim_reads = 0; //trigger interim reads just once

      sendData("Start sending Dust sensor data to server\n");

      if (dust_read_count > 0) {
          pm_1_0_value = (dust_total_pm_1_0_value / dust_read_count);
          pm_2_5_value = (dust_total_pm_2_5_value / dust_read_count);
          pm_10_0_value = (dust_total_pm_10_0_value / dust_read_count);
        
          sendSensorData(DUST_SENSOR_ID, SENSOR_PM1_0_TYPE, pm_1_0_value);
          sendSensorData(DUST_SENSOR_ID, SENSOR_PM2_5_TYPE, pm_2_5_value);
          sendSensorData(DUST_SENSOR_ID, SENSOR_PM10_0_TYPE, pm_10_0_value);
      } else {
          sendData("Failed to read dust sensor data\n");  
      }
    
      powerDustSensor(LOW);
    }
    
}

void powerDustSensor(uint8_t state) {
    digitalWrite(PLANTOWER_SET_PIN, state);
}

bool pms7003_read() {
    // send data only when you receive data:
    bool packetReceived = false;
    while (!packetReceived) {
        if (Serial.available() > 32) {
            int drain = Serial.available();
            if (DEBUG) {
                sendData("-- Draining buffer: ");
            }
            for (int i = drain; i > 0; i--) {
                Serial.read();
            }
        }
        
        if (Serial.available() > 0) {
            if (DEBUG) {
                sendData("-- Available: ");
            }
            incomingByte = Serial.read();
            if (DEBUG) {
                sendData("-- READ: ");
            }
            if (!inFrame) {
                if (incomingByte == 0x42 && detectOff == 0) {
                    frameBuf[detectOff] = incomingByte;
                    thisFrame.frameHeader[0] = incomingByte;
                    calcChecksum = incomingByte; // Checksum init!
                    detectOff++;
                }
                else if (incomingByte == 0x4D && detectOff == 1) {
                    frameBuf[detectOff] = incomingByte;
                    thisFrame.frameHeader[1] = incomingByte;
                    calcChecksum += incomingByte;
                    inFrame = true;
                    detectOff++;
                }
                else {
                    sendData("-- Frame syncing... ");
                    //Serial.print(incomingByte, HEX);
                }
            }
            else {
                frameBuf[detectOff] = incomingByte;
                calcChecksum += incomingByte;
                detectOff++;
                uint16_t val = frameBuf[detectOff-1]+(frameBuf[detectOff-2]<<8);
                switch (detectOff) {
                    case 4:
                        thisFrame.frameLen = val;
                        frameLen = val + detectOff;
                        break;
                    case 6:
                        thisFrame.concPM1_0_CF1 = val;
                        break;
                    case 8:
                        thisFrame.concPM2_5_CF1 = val;
                        break;
                    case 10:
                        thisFrame.concPM10_0_CF1 = val;
                        break;
                    case 12:
                        thisFrame.concPM1_0_amb = val;
                        break;
                    case 14:
                        thisFrame.concPM2_5_amb = val;
                        break;
                    case 16:
                        thisFrame.concPM10_0_amb = val;
                        break;
                    case 18:
                        thisFrame.rawGt0_3um = val;
                        break;
                    case 20:
                        thisFrame.rawGt0_5um = val;
                        break;
                    case 22:
                        thisFrame.rawGt1_0um = val;
                        break;
                    case 24:
                        thisFrame.rawGt2_5um = val;
                        break;
                    case 26:
                        thisFrame.rawGt5_0um = val;
                        break;
                    case 28:
                        thisFrame.rawGt10_0um = val;
                        break;
                    case 29:
                        val = frameBuf[detectOff-1];
                        thisFrame.version = val;
                        break;
                    case 30:
                        val = frameBuf[detectOff-1];
                        thisFrame.errorCode = val;
                        break;
                    case 32:
                        thisFrame.checksum = val;
                        calcChecksum -= ((val>>8)+(val&0xFF));
                        break;
                    default:
                        break;
                }
    
                if (detectOff >= frameLen) {
                    /* sprintf(printbuf, "PMS7003 \n");
                    sprintf(printbuf, "%s[%02x %02x] (%04x) \n", printbuf,
                        thisFrame.frameHeader[0], thisFrame.frameHeader[1], thisFrame.frameLen);
                    sprintf(printbuf, "%sCF1=[%d %d %d] \n", printbuf,
                        thisFrame.concPM1_0_CF1, thisFrame.concPM2_5_CF1, thisFrame.concPM10_0_CF1);
                    sprintf(printbuf, "%sCF1=[%04x %04x %04x] \n", printbuf,
                        thisFrame.concPM1_0_CF1, thisFrame.concPM2_5_CF1, thisFrame.concPM10_0_CF1);
                    sprintf(printbuf, "%samb=[%04x %04x %04x] \n", printbuf,
                        thisFrame.concPM1_0_amb, thisFrame.concPM2_5_amb, thisFrame.concPM10_0_amb);
                    sprintf(printbuf, "%sraw=[%04x %04x %04x %04x %04x %04x] \n", printbuf,
                        thisFrame.rawGt0_3um, thisFrame.rawGt0_5um, thisFrame.rawGt1_0um,
                        thisFrame.rawGt2_5um, thisFrame.rawGt5_0um, thisFrame.rawGt10_0um);
                    sprintf(printbuf, "%sver=%02x err=%02x \n", printbuf,
                        thisFrame.version, thisFrame.errorCode);
                    sprintf(printbuf, "%scsum=%04x %s xsum=%04x \n", printbuf,
                        thisFrame.checksum, (calcChecksum == thisFrame.checksum ? "==" : "!="), calcChecksum);
                    sendData(printbuf); */

                    packetReceived = true;
                    detectOff = 0;
                    inFrame = false;
                }
            }
        }
    }

    return (calcChecksum == thisFrame.checksum);
}

// ============================================================================================================================
// =================================================== Radiation Sensor Functions =============================================
// ============================================================================================================================
void handleRadiationSensor() {
    unsigned long currentTime = millis();
    
    // Start read and send cycle
    if ( (timer_rad_sensor_start > 0) && (timer_rad_sensor_start < currentTime) ) {
      sendData("Start to read and send Radiation Sensor Data\n");
      timer_rad_sensor_start = currentTime + ((RAD_SENSOR_READ_AND_SEND_INTERVAL_MIN * 60) * 1000);
      timer_rad_sensor_send = currentTime + (RAD_SENSOR_AVG_INTERVAL_SEC * 1000);
      radiation_count = 0;
    }
    
    // AVG read
    if ( (timer_rad_sensor_send > 0) && (timer_rad_sensor_send < currentTime) ) {
      timer_rad_sensor_send = 0;

      if (radiation_count > 0) {
          char buffer[255];
          sprintf(buffer, "RADIATION(CPM)=%d\n", radiation_count);
          sendData(buffer);

          radiation_value = radiation_count;
          sendSensorData(RAD_SENSOR_ID, SENSOR_RCPM_TYPE, radiation_value);
      } else {
          sendData("Failed to read Radiation Sensor Data\n");  
      }
    }
   
}

void incrementRadiationCount() {
  radiation_count++;
}

// ============================================================================================================================
// =================================================== Common Functions =======================================================
// ============================================================================================================================
void sendData(char writeBuffer[], unsigned int len) {
    //Udp.beginPacket(udpRemoteIP, udpRemotePort);
    //Udp.write(writeBuffer, len);
    //Udp.endPacket();
}

void sendData(char writeBuffer[]) {
    //Udp.beginPacket(udpRemoteIP, udpRemotePort);
    //Udp.write(writeBuffer);
    //Udp.endPacket();
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  sendData("MQTT Connecting...");

  uint8_t retries = 10;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       // sendData(mqtt.connectErrorString(ret));
       sendData("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000); // wait 2 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  
  sendData("MQTT Connected!");
}

void sendSensorData(const char *sensor, const char *type, uint32_t value) {
    /* char url[255];
    sprintf(url, "http://192.168.1.116/misc/save_sensor_data_http.php?sensor=%d&type=%d&value=%d", sensor, type, value);
    
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    
    if(httpCode > 0) {
        sendData("Data sent\n");
    } else {
        sendData("Failed to send sensor data over http\n");
    }

    http.end(); */

    char data[127];
    sprintf(data, "{\"sensor\": \"%s\",\"channel\": \"%s\",\"value\": %d}", sensor, type, value);

    if (!sensorPublisher.publish(data)) {
      sendData("MQTT push failed\n");
    } else {
      sendData("MQTT push successfull\n");
    }
}
