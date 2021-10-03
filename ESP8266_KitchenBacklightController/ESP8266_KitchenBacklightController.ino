#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiConfig.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h> 

// https://www.esp8266.com/wiki/doku.php?id=esp8266_gpio_pin_allocations
#define GPIO_POUT_PWD_FREQUENCY 500
#define GPIO_POUT 0 // GPIO0
#define GPIO_PIN 3 // GPIO3 - RX

#define UNKNOWN_STATE 10
volatile int port_input_value = UNKNOWN_STATE;

#define LED_OFF      0
#define LED_ON       1
#define LED_COMPLETE 2

#define LED_MAX_VALUE_LOW 100
#define LED_MAX_VALUE_HIGH 1023

#define EEPROM_START_ADDR 0

#define AUTO_SHUTOFF_PERIOD_LOW 1
#define AUTO_SHUTOFF_PERIOD_HIGH 120

volatile char led_state = LED_COMPLETE;
volatile int pmw_level = 0;
volatile int pmw_max_level = 1023;
volatile unsigned long last_led_time = 0;
volatile unsigned long auto_shutoff_time = 0;
volatile int auto_shutoff_period = 30;

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

void indexPage() {
  String message = "<!doctype html>";
  message += "<html lang=\"en\">";
  message += "<head>";
  message += "<title>ESP8266 New Kitchen Backlight Controller v0.3</title>";
  message += "</head>";

  message += "<body>";
  
  message += "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  message += "--------------------------------------------------------------<br/>";
  
  //message += "<a href=\"/switchOn\">switchOn</a><br/>";
  //message += "<a href=\"/switchOff\">switchOff</a><br/>";
  //message += "<a href=\"/switchToggle\">switchToggle</a><br/>";
  //message += "<a href=\"/setLevel\">setLevel</a><br/>";
  //message += "<a href=\"/status\">status</a><br/>";
  
  //message += "--------------------------------------------------------------<br/>";
  
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

  message += "--------------------------------------------------------------<br/>";
  
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
  setupGPIO();

  initEEPROM();
  
  //Serial.begin(115200);
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
  
  WiFi.mode(WIFI_STA);

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

  setupHttpHandlers();

  httpUpdater.setup(&server);

  server.begin();
  
  Serial.println("HTTP server started");  
}

void initEEPROM() {
  EEPROM.begin(512);

  // Read previous value
  EEPROM.get(EEPROM_START_ADDR, pmw_max_level);

  // In case value read is out of range apply correction and save to EEPROM
  if ((pmw_max_level < LED_MAX_VALUE_LOW) || (pmw_max_level > LED_MAX_VALUE_HIGH)) {
    pmw_max_level = LED_MAX_VALUE_HIGH;
    EEPROM.put(EEPROM_START_ADDR, pmw_max_level);
  }

  // Read previous value
  EEPROM.get(EEPROM_START_ADDR + sizeof(int), auto_shutoff_period);

  // In case value read is out of range apply correction and save to EEPROM
  if ((auto_shutoff_period < AUTO_SHUTOFF_PERIOD_LOW) || (auto_shutoff_period > AUTO_SHUTOFF_PERIOD_HIGH)) {
    auto_shutoff_period = AUTO_SHUTOFF_PERIOD_HIGH;
    EEPROM.put(EEPROM_START_ADDR + sizeof(int), auto_shutoff_period);
  }

  EEPROM.commit();
}

void setupGPIO() {
  analogWriteFreq(GPIO_POUT_PWD_FREQUENCY);
  pinMode(GPIO_POUT, OUTPUT);

  //pinMode(GPIO_PIN, INPUT);
  //GPIO 3 (RX) swap the pin to a GPIO.
  // https://arduino.stackexchange.com/questions/29938/how-to-i-make-the-tx-and-rx-pins-on-an-esp-8266-01-into-gpio-pins
  pinMode(GPIO_PIN, FUNCTION_3); 
}

void setupHttpHandlers() {
  server.on("/", indexPage);

  server.on("/setMaxLevel", [](){
    String valueStr = server.arg("value");
    int value = valueStr.toInt();

    if ((value >= LED_MAX_VALUE_LOW) && (value <= LED_MAX_VALUE_HIGH)) {
      // set new max level
      pmw_max_level = value;
      // apply changes
      led_state = LED_ON;
      
      // set to EEPROM value
      EEPROM.put(EEPROM_START_ADDR, pmw_max_level);
      EEPROM.commit();
      
      server.send(200, "text/plain", "OK"); 
    } else {
      server.send(400, "text/plain", "Invalid value"); 
    }
  });

  server.on("/getMaxLevel", [](){
    String content = String(pmw_max_level);
    server.send(200, "text/plain", content);
  });

  server.on("/setAutoShutOffPeriod", [](){
    String valueStr = server.arg("value");
    int value = valueStr.toInt();

    if ((value >= AUTO_SHUTOFF_PERIOD_LOW) && (value <= AUTO_SHUTOFF_PERIOD_HIGH)) {
      // set new max level
      auto_shutoff_period = value;
      
      // set to EEPROM value
      EEPROM.put(EEPROM_START_ADDR + sizeof(int), auto_shutoff_period);
      EEPROM.commit();
      
      server.send(200, "text/plain", "OK"); 
    } else {
      server.send(400, "text/plain", "Invalid value"); 
    }
  });

  server.on("/getAutoShutOffPeriod", [](){
    String content = String(auto_shutoff_period);
    server.send(200, "text/plain", content);
  });

  server.on("/setLevel", [](){
    String valueStr = server.arg("value");
    int value = valueStr.toInt();
    analogWrite(GPIO_POUT, value);
    server.send(200, "text/plain", "OK"); 
  });

  server.on("/getLevel", [](){
    String content = String(pmw_level);
    server.send(200, "text/plain", content);
  });

  server.on("/setLedState", [](){
    String valueStr = server.arg("value");
    int value = valueStr.toInt();
    led_state = (value) ? LED_ON : LED_OFF;
    reset_auto_shutoff();
    server.send(200, "text/plain", "OK"); 
  });

  server.on("/toggle", [](){
    char current_led_state = led_state;
    
    if (current_led_state == LED_COMPLETE) {
      led_state = (pmw_level == 0) ? LED_ON : LED_OFF;
    } else {
      led_state = (led_state == LED_ON) ? LED_OFF : LED_ON;
    }
    
    server.send(200, "text/plain", "OK");
  });

  server.onNotFound(handleNotFound);  
}

void handleInputPortChange() {
  int input = digitalRead(GPIO_PIN);

  if (port_input_value == input) {
    // If no change detected exit
    return;
  }

  port_input_value = input;
  
  if (input == HIGH) {
    led_state = LED_OFF;
  } else {
    led_state = LED_ON;
  }
}

void changeLedState() {
  if (led_state == LED_COMPLETE) {
    // exit if LED status complete
    return;
  }
  
  unsigned long currentTime = micros();

  if ((currentTime - last_led_time) < 500) {
    // not the time yet
    return;
  }

  last_led_time = currentTime;

  switch(led_state) {
    // ----------- LED OFF ----------- 
    case LED_OFF:

      if (pmw_level == 0) {
        led_state = LED_COMPLETE;
      } else {
        pmw_level = pmw_level - 1;  
      }

      break;

    // ----------- LED ON ----------- 
    case LED_ON:

      if (pmw_level >= pmw_max_level) {
        pmw_level = pmw_max_level;
        led_state = LED_COMPLETE;
      } else {
        pmw_level = pmw_level + 1;  
      }
      
      break;
  }

  analogWrite(GPIO_POUT, pmw_level);
}

void reset_auto_shutoff() {
  auto_shutoff_time = millis();
}

void autoShutOff() {
  if (pmw_level < 200) {
    reset_auto_shutoff();
    
    // led is not turned on
    return;
  }

  unsigned long currentTime = millis();

  if ((currentTime - auto_shutoff_time) < (auto_shutoff_period * 60000)) {
    // time is not yet up, exit
    return;  
  }

  led_state = LED_OFF;
}

void loop(void){
  server.handleClient();

  handleInputPortChange();

  changeLedState();

  autoShutOff();
}
