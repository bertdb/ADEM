/**********************************************/
/* Building Arduino Dust Sensor using:        */
/*      - ESP8266 ESP-01                      */
/*      - 3.3-to-5v Logic Level Converter     */
/*      - Shinyei PPD42NS                     */
/* http://www.sca-shinyei.com/pdf/PPD42NS.pdf */
/*                                            */
/* Author: shadowandy[dot]sg[at]gmail[dot]com */
/* Web: www.shadowandy.net                    */
/*                                            */         
/* Wiring Instruction:                        */
/*      - PPD42NS Pin 1 => GND                */
/*      - PPD42NS Pin 2 => TX                 */
/*      - PPD42NS Pin 3 => 5V                 */
/*      - PPD42NS Pin 4 => RX                 */
/**********************************************/
#include <ESP8266WiFi.h>                 
#include <TinyGPS++.h>
#include "config.h"

const char ssid[] = WIFISSID;
const char pass[] = WIFIPW;
const char thingSpeakAddress[] = THINGADDR;
const char thingSpeakAPIKey[] = THINGKEY;

static const uint32_t GPSBaud = 9600;
// The TinyGPS++ object
TinyGPSPlus gps;


#define PM25 0
#define PM10 1
int pin[] = {12, 1};
//int pin[] = {0, 3};
unsigned long starttime;
unsigned long sampletime_ms = 10000;
unsigned long sleeptime_ms = 255000000;
unsigned long triggerOn[2];
unsigned long triggerOff[2];
unsigned long lowpulseoccupancy[] = {0, 0};
float ratio[] = {0, 0};
float count[] = {0, 0};
boolean value[] = {HIGH, HIGH};
boolean trigger[] = {false, false};
float latitude = 0.0;
float longitude = 0.0;
int hours = 0;
int minutes = 0;
int seconds = 0;
int year = 0;
int month = 0;
int day = 0;
int chipId = 0;



void setup() {
  //connectWiFi();
  Serial1.begin(9600);
  Serial.begin(GPSBaud);
  chipId = ESP.getChipId();
  Serial1.print(F("start "));
  Serial1.println(chipId);

  

  pinMode(pin[PM25], FUNCTION_3); //Set TX PIN to GPIO
  pinMode(pin[PM10], FUNCTION_3); //Set RX PIN to GPIO
  pinMode(pin[PM25], INPUT_PULLUP); //Listen at the designated PIN
  attachInterrupt(pin[PM25], intrLOPM25, CHANGE); // Attaching interrupt to PIN
  pinMode(pin[PM10], INPUT_PULLUP); //Listen at the designated PIN
  attachInterrupt(pin[PM10], intrLOPM10, CHANGE); // Attaching interrupt to PIN
  //connectWiFi();
  starttime = millis(); //Fetching the current time
  ESP.wdtEnable(WDTO_8S); // Enabling Watchdog
}

void loop() {
  
  ESP.wdtFeed(); // Reset the WatchDog
  
  if ((millis() - starttime) > sampletime_ms) //Checking if it is time to sample
  {
    unsigned long sampledtime = millis() - starttime;
    ratio[PM25] = lowpulseoccupancy[PM25] / (sampledtime * 10.0);
    count[PM25] = 1.1 * pow(ratio[PM25], 3) - 3.8 * pow(ratio[PM25], 2) + 520 * ratio[PM25] + 0.62;
    ratio[PM10] = lowpulseoccupancy[PM10] / (sampledtime * 10.0);
    count[PM10] = 1.1 * pow(ratio[PM10], 3) - 3.8 * pow(ratio[PM10], 2) + 520 * ratio[PM10] + 0.62;
    count[PM25] -= count[PM10];
    
    ESP.wdtFeed(); // Reset the WatchDog
    // Begin mass concentration calculation
    float concentration[] = {0, 0};
    double pi = 3.14159;
    double density = 1.65 * pow(10, 12);
    double K = 3531.5;
    
    ESP.wdtFeed(); // Reset the WatchDog
    // PM10
    double r10 = 2.6 * pow(10, -6);
    double vol10 = (4 / 3) * pi * pow(r10, 3);
    double mass10 = density * vol10;
    concentration[PM10] = (count[PM10]) * K * mass10;
    
    ESP.wdtFeed(); // Reset the WatchDog
    // PM2.5
    double r25 = 0.44 * pow(10, -6);
    double vol25 = (4 / 3) * pi * pow(r25, 3);
    double mass25 = density * vol25;
    concentration[PM25] = (count[PM25]) * K * mass25;
    // End of mass concentration calculation

    ESP.wdtFeed(); // Reset the WatchDog

      while (Serial.available() > 0)
        if (gps.encode(Serial.read()))
          returnGpsInfo();

    
    connectWiFi();
    // Data die momenteel doorgestuurd wordt naar ThingSpeak.
    updateThingSpeak("1=" + String(concentration[PM10], DEC) + "&2=" + String(count[PM10], DEC) + "&3=" + 
      String(concentration[PM25], DEC) + "&4=" + String(count[PM25], DEC) + "&5=" + String(latitude) + 
      "&6=" + String(longitude) + "&7=" + String(seconds));
    // Sleeping until the next sampling
    ESP.wdtDisable();
    //delay(sleeptime_ms);
    ESP.wdtEnable(WDTO_8S);
    lowpulseoccupancy[PM25] = 0;
    lowpulseoccupancy[PM10] = 0;
    ESP.deepSleep(sleeptime_ms, WAKE_RF_DEFAULT);
    // Resetting for next sampling
    //lowpulseoccupancy[PM25] = 0;
    //lowpulseoccupancy[PM10] = 0;
    //starttime = millis();
    //ESP.wdtFeed(); // Reset the WatchDog
  }
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void updateThingSpeak(String tsData) {
  WiFiClient client;
  if (!client.connect(thingSpeakAddress, 80)) {
    return;
  }
  client.print(F("GET /update?key="));
  client.print(thingSpeakAPIKey);
  client.print(F("&"));
  client.print(tsData);
  client.print(F(" HTTP/1.1\r\nHost: api.thingspeak.com\r\n\r\n"));
  client.println();
  Serial1.println(tsData);
}

void intrLOPM25() {
  value[PM25] = digitalRead(pin[PM25]);
  if (value[PM25] == LOW && trigger[PM25] == false) {
    trigger[PM25] = true;
    triggerOn[PM25] = micros();
  }
  if (value[PM25] == HIGH && trigger[PM25] == true) {
    triggerOff[PM25] = micros();
    lowpulseoccupancy[PM25] += (triggerOff[PM25] - triggerOn[PM25]);
    trigger[PM25] = false;
  }
}

void intrLOPM10() {
  value[PM10] = digitalRead(pin[PM10]);
  if (value[PM10] == LOW && trigger[PM10] == false) {
    trigger[PM10] = true;
    triggerOn[PM10] = micros();
  }
  if (value[PM10] == HIGH && trigger[PM10] == true) {
    triggerOff[PM10] = micros();
    lowpulseoccupancy[PM10] += (triggerOff[PM10] - triggerOn[PM10]);
    trigger[PM10] = false;
  }
}

void returnGpsInfo()
{
  if (gps.location.isValid())
  {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
  }
  else
  {
    latitude = 1.0;
    longitude = 2.0;
  }

  if (gps.date.isValid())
  {
    month = gps.date.month();
    day = gps.date.day();
    year = gps.date.year();
  }
  else
  {
    month = 0;
    day = 0;
    year = 0;
  }

  if (gps.time.isValid())
  {
    hours = gps.time.hour();
    minutes = gps.time.minute();
    seconds = gps.time.second();
  }
  else
  {
    hours = 0;
    minutes = 0;
    seconds = 3;
  }

}
