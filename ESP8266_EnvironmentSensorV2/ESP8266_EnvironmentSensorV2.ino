#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266HTTPUpdateServer.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include <PMS.h>

const char *ssid = "EnvironmentSensorV2";

std::unique_ptr<ESP8266WebServer> server;
ESP8266HTTPUpdateServer httpUpdater;

#define OLED_MOSI   0
#define OLED_CLK    2
#define OLED_DC     5
#define OLED_CS    16
#define OLED_RESET  4

Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

#define SEALEVELPRESSURE_HPA (1013.25)

#define BME280_SDA 12
#define BME280_SCL 13

volatile float temperature = 0;
volatile float pressure = 0;
volatile float humidity = 0;

Adafruit_BME280 bme;

#define PLANTOWER_SET_PIN  15
#define PLANTOWER_RST_PIN  14

volatile uint16_t PM_AE_UG_1_0 = 0;
volatile uint16_t PM_AE_UG_2_5 = 0;
volatile uint16_t PM_AE_UG_10_0 = 0;

PMS pms(Serial);
PMS::DATA data;

#define SENSOR_PMS_READ_INTERVAL_SEC         300
#define SENSOR_PMS_READ_INIT_SEC              30

#define SENSOR_BME_READ_INTERVAL_SEC          10

volatile unsigned long timer_bme_sensor_read = 0;

volatile unsigned long timer_pms_sensor_read = 0;
volatile unsigned long timer_pms_sensor_init = 0;

volatile bool startPMSCountDown = false;
volatile bool softAP = false;


void handleRoot() {
  String message = "";

  // see docs/template.html
  
  message =  "<!doctype html>                                                                                                                                                                                                                                  ";
  message += "<html lang='en'>                                                                                                                                                                                                                                 ";
  message += "  <head>                                                                                                                                                                                                                                         ";
  message += "  <title>Environment Sensor V2</title>                                                                                                                                                                                                           ";
  message += "  <style>                                                                                                                                                                                                                                        ";
  message += "  .box {padding: 10px;border: dashed 1px black;margin-top: 10px;background-color: #90b3ed;} h2 {margin: 0;} span.desc {font-style: italic;font-size: 12px;} form {margin: 10px 0 0 0; padding: 10px; background-color: #f4df42;}                 ";
  message += "  span.val {width: 100px;display: inline-block;}                                                                                                                                                                                                 ";
  message += "  </style>  ";
  message += "  </head>                                                                                                                                                                                                                                        ";
  message += "  <body>                                                                                                                                                                                                                                         ";
  message += "                                                                                                                                                                                                                                                 ";
  message += "  <div class='box'><h2>Envirnment Sensor V2</h2></div>                                                                                                                                                                                           ";
  message += "                                                                                                                                                                                                                                                 ";
  message += "  <div class='box'><h2>Sensor Data</h2>                                                                                                                                                                                                          ";
  message += "  <span class='val'>Temperature:</span>                                                                                                                                                                                                          ";
  message.concat(temperature); message.concat(" C</br>");
  message += "  <span class='val'>Humidity:</span>                                                                                                                                                                                                             ";
  message.concat(humidity); message.concat(" %</br>");
  message += "  <span class='val'>Pressure:</span>                                                                                                                                                                                                             ";
  message.concat(pressure); message.concat(" hPa</br>");
  message += "  <span class='val'>PM 1.0:</span>                                                                                                                                                                                                               ";
  message.concat(PM_AE_UG_1_0); message.concat(" (ug/m3)</br>");
  message += "  <span class='val'>PM 2.5:</span>                                                                                                                                                                                                               ";
  message.concat(PM_AE_UG_2_5); message.concat(" (ug/m3)</br>");
  message += "  <span class='val'>PM10.0:</span>                                                                                                                                                                                                               ";
  message.concat(PM_AE_UG_10_0); message.concat(" (ug/m3)</br>");
  message += "  <a href='sensor'>Get Sensor Data (available as JSON and JSONP)</a>                                                                                                                                                                             ";
  message += "  </div>                                                                                                                                                                                                                                         ";
  message += "                                                                                                                                                                                                                                                 ";
  message += "  <div class='box'>                                                                                                                                                                                                                              ";
  message += "  <h2>WiFi Settings</h2>                                                                                                                                                                                                                         ";
  message += "  Current Signal Strength:                                                                                                                                                                                                                       ";
  message.concat(WiFi.RSSI()); message.concat("</br>");
  message += "  <span class='desc'>                                                                                                                                                                                                                            ";
  message += "    Signal Strength TL;DR   Required for<br/>                                                                                                                                                                                                    ";
  message += "    -30 dBm Amazing Max achievable signal strength. The client can only be a few feet from the AP to achieve this. Not typical or desirable in the real world.  N/A<br/>                                                                         ";
  message += "    -67 dBm Very Good Minimum signal strength for applications that require very reliable, timely delivery of data packets. VoIP/VoWiFi, streaming video<br/>                                                                                    ";
  message += "    -70 dBm Okay  Minimum signal strength for reliable packet delivery. Email, web<br/>                                                                                                                                                          ";
  message += "    -80 dBm Not Good  Minimum signal strength for basic connectivity. Packet delivery may be unreliable.  N/A<br/>                                                                                                                               ";
  message += "    -90 dBm Unusable  Approaching or drowning in the noise floor. Any functionality is highly unlikely. N/A<br/>                                                                                                                                 ";
  message += "  </span>                                                                                                                                                                                                                                        ";
  message += "  <a href='reset'>Reset WiFi configuration</a><br/>                                                                                                                                                                                              ";
  message += "  </div>                                                                                                                                                                                                                                         ";
  message += "                                                                                                                                                                                                                                                 ";
  message += "  <div class='box'>                                                                                                                                                                                                                              ";
  message += "  <h2>Firmware:</h2>                                                                                                                                                                                                                             ";
  message += "  <a href='https://github.com/aformusatii/ESP8266Arduino/tree/master/ESP8266_EnvironmentSensorV2'>Git Repository</a></br>                                                                                                                        ";
  message += "  <form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>                                                                                                ";
  message += "  </div>                                                                                                                                                                                                                                         ";
  message += "                                                                                                                                                                                                                                                 ";
  message += "  </body>                                                                                                                                                                                                                                        ";
  message += "</html>                                                                                                                                                                                                                                          ";

  server->send(200, "text/html", message);
}

void resetWiFiManager() {
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  
  server->send(200, "text/html", "<h1>WiFiManager was reset.</h1>");
}

void initHttpServer() {
  server.reset(new ESP8266WebServer(WiFi.localIP(), 80));

  server->on("/", handleRoot);
  server->on("/reset", resetWiFiManager);

  server->on("/sensor", [](){
    char buf[255];
    String callback = server->arg("callback");

    if (callback.length() > 0) {
      sprintf(buf, "%s({\"temperature\": %.2f, \"humidity\": %.2f, \"pressure\": %.2f, \"pm_1_0\": %u, \"pm_2_5\": %u, \"pm_10_0\": %u});",
        callback.c_str(),
        temperature,
        humidity,
        pressure,
        PM_AE_UG_1_0,
        PM_AE_UG_2_5,
        PM_AE_UG_10_0);
  
      server->send(200, "text/plain", buf);
    } else {
      sprintf(buf, "{\"temperature\": %.2f, \"humidity\": %.2f, \"pressure\": %.2f, \"pm_1_0\": %u, \"pm_2_5\": %u, \"pm_10_0\": %u}",
        temperature,
        humidity,
        pressure,
        PM_AE_UG_1_0,
        PM_AE_UG_2_5,
        PM_AE_UG_10_0);
        
      server->send(200, "application/json", buf);      
    }
    
  });

  httpUpdater.setup(server.get());
  
  server->begin();
}

void initBME280() {
  Wire.begin(BME280_SDA, BME280_SCL);
  
  bool status = bme.begin(0x76);
  if (!status) {
      // Serial.println("Could not find a valid BME280 sensor, check wiring!");
  }
}

void initPMS5003() {
  pinMode(PLANTOWER_SET_PIN, OUTPUT);
  digitalWrite(PLANTOWER_SET_PIN, HIGH);

  pinMode(PLANTOWER_RST_PIN, OUTPUT);
  digitalWrite(PLANTOWER_RST_PIN, HIGH);

  pms.passiveMode();
}

void refreshDisplay() {
    display.clearDisplay();
    display.setCursor(0,0);

    display.print("Temperature: ");
    display.print(temperature);
    display.println(" *C");

    display.print("Pressure: ");
    display.print(pressure);
    display.println(" hPa");

    display.print("Humidity: ");
    display.print(humidity);
    display.println(" %");

    display.print("PM 1.0: ");
    display.print(PM_AE_UG_1_0);
    display.println(" (ug/m3)");

    display.print("PM 2.5: ");
    display.print(PM_AE_UG_2_5);
    display.println(" (ug/m3)");

    display.print("PM10.0: ");
    display.print(PM_AE_UG_10_0);
    display.println(" (ug/m3)");

    if (startPMSCountDown) {
      display.println("Loading PMS... ");
    } else {
      display.println("Updated PMS. ");
    }

    display.print("IP: ");
    display.println(softAP ? WiFi.softAPIP() : WiFi.localIP());
    
    display.display();
}

void bmeSensorRead() {
  unsigned long currentTime = millis();

  if ( (timer_bme_sensor_read > 0) && (currentTime > timer_bme_sensor_read) ) {
    timer_bme_sensor_read = currentTime + (SENSOR_BME_READ_INTERVAL_SEC * 1000);

    temperature = bme.readTemperature(); 
    pressure = bme.readPressure() / 100.0F;
    humidity = bme.readHumidity();
  
    refreshDisplay();  
  }

}

void pmsSensorRead() {
  unsigned long currentTime = millis();

  if ( (timer_pms_sensor_read > 0) && (currentTime > timer_pms_sensor_read) ) {
    timer_pms_sensor_read = currentTime + (SENSOR_PMS_READ_INTERVAL_SEC * 1000);
    timer_pms_sensor_init = currentTime + (SENSOR_PMS_READ_INIT_SEC * 1000);

    pms.wakeUp();

    startPMSCountDown = true;

    refreshDisplay();
  }

  if ( (timer_pms_sensor_init > 0) && (currentTime > timer_pms_sensor_init) ) {
    timer_pms_sensor_init = 0;
    
    startPMSCountDown = false;
    
    pms.requestRead();

    if (pms.read(data, 3000)) {
      PM_AE_UG_1_0 = data.PM_AE_UG_1_0;
      PM_AE_UG_2_5 = data.PM_AE_UG_2_5;
      PM_AE_UG_10_0 = data.PM_AE_UG_10_0;

      refreshDisplay();
    }

    pms.sleep();
  }
}

void setup() {
  delay(1000);

  Serial.begin(9600);

  initBME280();
  initPMS5003();

  display.begin(SSD1306_SWITCHCAPVCC);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  display.setCursor(0,0);
  display.println("Environment Sensor V2");
  display.display();

  delay(1000);

  display.println("Load WiFi Config...");
  display.println("Connect to ssid: ");
  display.println(ssid);
  display.println("You have 2 minutes.");
  display.display();
  
  WiFiManager wifiManager;
  wifiManager.setTimeout(120);

  if (!wifiManager.autoConnect(ssid)) {
    display.println("Failed to connect.");
    display.println("Continue in AP mode.");
    display.display();    

    delay(2000);

    softAP = true;
  }

  initHttpServer();

  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Connected to WiFi...");
  display.print("IP: ");
  display.println(softAP ? WiFi.softAPIP() : WiFi.localIP());
  display.display();

  delay(2000);

  timer_bme_sensor_read = 1;
  timer_pms_sensor_read = 1;
  timer_pms_sensor_init = 0;
}

void loop() {
  server->handleClient();

  bmeSensorRead();

  pmsSensorRead();
}
