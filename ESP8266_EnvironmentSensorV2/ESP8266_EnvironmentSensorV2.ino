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
  String message = "<!doctype html>";
  message += "<html lang=\"en\">";
  message += "<head>";
  message += "<title>Environment Sensor V2</title>";
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

  message += "<a href=\"/reset\">Reset WiFi configuration</a><br/>";
  message += "<a href=\"/sensor\">Get Sensor Data</a><br/>";

  message += "Firmware:<br/>";

  message += "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  
  message += "</body>";  
  message += "</html>";

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

    if (pms.read(data, 1000)) {
      PM_AE_UG_1_0 = data.PM_AE_UG_1_0;
      PM_AE_UG_2_5 = data.PM_AE_UG_2_5;
      PM_AE_UG_10_0 = data.PM_AE_UG_10_0;

      pms.sleep();

      refreshDisplay();
    }
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
