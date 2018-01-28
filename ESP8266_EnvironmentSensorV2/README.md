Wireless Environment Sensor.

###### Components:
 
| Component                                                                                                     | Description                               | Price  |
|---------------------------------------------------------------------------------------------------------------|-------------------------------------------|--------|
| [PMS5003](http://www.plantower.com/en/content/?108.html), compatible with PMS3003 and PMS7003                 | Dust Sensor                               | $14.80 |
| [BME280](https://www.bosch-sensortec.com/bst/products/all_products/bme280)                                    | Temperature, Humidity and Pressure Sensor | $4.26  |
| [ESP8266](https://en.wikipedia.org/wiki/ESP8266)###### Specifications:                                        | WiFi microchip/microcontroller            | $3.85  |
| [OLED module based on SSD1306](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf) - Power supply: 5V 500mA | Display Module                            | $3     |
| [AMS1117](http://www.advanced-monolithic.com/pdf/ds1117.pdf)                                                  | 1A LOW DROPOUT VOLTAGE REGULATOR          | $0.1   |
| Misc parts: Prototype PCB, electrolytic capacitors                                                            |                                           | $1     |

Total price: ~$27
  
###### Collected data:
 - Dust sensor (PM 1.0, PM 2.5, PM10.0)
 - Humidity
 - Temperature
 - Atmospheric Pressure

###### Software/Libraries:
 - [Arduino IDE v1.8.5](https://www.arduino.cc/en/Main/Software)
 - [Arduino core for ESP8266 WiFi chip](https://github.com/esp8266/Arduino)
 - [Adafruit GFX Library] (https://github.com/adafruit/Adafruit-GFX-Library)
 - [Adafruit SSD1306 Library] (https://github.com/adafruit/Adafruit_SSD1306) Note: you will need to modify Adafruit_SSD1306.h to enable 128x64 resolution.
 - [Adafruit Unified Sensor Driver] (https://github.com/adafruit/Adafruit_Sensor)
 - [Adafruit BME280 Library](https://github.com/adafruit/Adafruit_BME280_Library)
 - [Arduino library for Plantower PMS sensors by Mariusz Kacki](https://github.com/fu-hsi/pms)

###### Circuit diagram:

![circuit diagram](docs/EMV2.png?raw=true "Sensor circuit diagram")

###### Prototype PCB:

![prototype pcb](docs/the_device.jpg?raw=true "Prototype PCB")

###### BME/BMP280 breakout board modification to work only with 3.3V:

![breakout board](docs/BME280Breakout.png?raw=true "Breakout Board")