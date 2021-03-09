/********************************************************************/
// First we include the libraries
#include <OneWire.h> 
#include <DallasTemperature.h>
/********************************************************************/
// Data wire is plugged into pin 2 on the Arduino 
#define ONE_WIRE_BUS 2
#define PIN_FAN_ON_OFF 3

#define FAN_CONTROL_INTERVAL_MS 2000 

const byte OC1A_PIN = 9;
const word PWM_FREQ_HZ = 20000; //Adjust this value to adjust the frequency
const word TCNT1_TOP = 16000000/(2*PWM_FREQ_HZ);

volatile unsigned long last_fan_time = 0;

/********************************************************************/
// Setup a oneWire instance to communicate with any OneWire devices  
// (not just Maxim/Dallas temperature ICs) 
OneWire oneWire(ONE_WIRE_BUS); 
/********************************************************************/
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
/********************************************************************/ 

void setup(void) 
{ 
  // start serial port 
  Serial.begin(9600); 
  Serial.println("Dallas Temperature IC Control Library Demo"); 
  // Start up the library 
  sensors.begin(); 
  setupGPIO();
  setupTimer();
} 

void setupGPIO() 
{
  pinMode(OC1A_PIN, OUTPUT);
  
  pinMode(PIN_FAN_ON_OFF, OUTPUT);
  digitalWrite(PIN_FAN_ON_OFF, HIGH);
}

void setupTimer()
{
  // Clear Timer1 control and count registers
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  TCCR1A |= (1 << COM1A1) | (1 << WGM11);
  TCCR1B |= (1 << WGM13) | (1 << CS10);
  ICR1 = TCNT1_TOP;

  setFanPwmDuty(100);
}

void setFanPwmDuty(byte duty) {
  if (duty == 0) {
    digitalWrite(PIN_FAN_ON_OFF, LOW);
  } else {
    digitalWrite(PIN_FAN_ON_OFF, HIGH);
    OCR1A = (word) (duty*TCNT1_TOP)/100;
  }
}

byte getDutyByTemperature(float temperatureC) {
  if (temperatureC < 0) {
    return 100;  
  }
  
  if (temperatureC > 70) {
    return 100;  
  }

  if (temperatureC > 60) {
    return 80;  
  }

  if (temperatureC > 50) {
    return 50;  
  }

  if (temperatureC > 45) {
    return 30;  
  }

  if (temperatureC > 40) {
    return 10;  
  }

  return 0;
}

void fanControlLoop() 
{
  unsigned long currentTime = millis();

  if ((currentTime - last_fan_time) < FAN_CONTROL_INTERVAL_MS) {
      // not the time yet
      return;
  }

  last_fan_time = currentTime;

  // do the actual stuff
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);

  byte fanDuty = getDutyByTemperature(temperatureC);
  setFanPwmDuty(fanDuty);

  Serial.println(temperatureC);
  Serial.println(fanDuty);
}

void loop(void) 
{ 
  fanControlLoop();
} 
