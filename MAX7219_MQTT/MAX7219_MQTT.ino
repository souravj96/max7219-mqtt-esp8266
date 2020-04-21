#include <ESP8266WiFi.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "EspMQTTClient.h"

#define MAX_DEVICES 4

#define CLK_PIN   D5 // or SCK
#define DATA_PIN  D7 // or MOSI
#define CS_PIN    D4 // or SS

#define HARDWARE_TYPE MD_MAX72XX::ICSTATION_HW
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

const uint8_t MESG_SIZE = 255;
const uint8_t CHAR_SPACING = 1;
uint8_t SCROLL_DELAY = 75;
uint8_t INTENSITY = 5;

char curMessage[MESG_SIZE];
char newMessage[MESG_SIZE];
bool newMessageAvailable = false;

void scrollDataSink(uint8_t dev, MD_MAX72XX::transformType_t t, uint8_t col) {}

uint8_t scrollDataSource(uint8_t dev, MD_MAX72XX::transformType_t t)
{
  static enum { S_IDLE, S_NEXT_CHAR, S_SHOW_CHAR, S_SHOW_SPACE } state = S_IDLE;
  static char   *p;
  static uint16_t curLen, showLen;
  static uint8_t  cBuf[8];
  uint8_t colData = 0;

  // finite state machine to control what we do on the callback
  switch (state)
  {
  case S_IDLE: 
    p = curMessage;
    if (newMessageAvailable) 
    {
      strcpy(curMessage, newMessage);
      newMessageAvailable = false;
    }
    state = S_NEXT_CHAR;
    break;

  case S_NEXT_CHAR: 
    if (*p == '\0')
      state = S_IDLE;
    else
    {
      showLen = mx.getChar(*p++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);
      curLen = 0;
      state = S_SHOW_CHAR;
    }
    break;

  case S_SHOW_CHAR: 
    colData = cBuf[curLen++];
    if (curLen < showLen)
      break;

    showLen = (*p != '\0' ? CHAR_SPACING : (MAX_DEVICES*COL_SIZE)/2);
    curLen = 0;
    state = S_SHOW_SPACE;

  case S_SHOW_SPACE:  
    curLen++;
    if (curLen == showLen)
      state = S_NEXT_CHAR;
    break;

  default:
    state = S_IDLE;
  }

  return(colData);
}

EspMQTTClient client(
  "AREA_51",
  "targetlocked",
  "192.168.1.111",  // MQTT Broker server ip
  "MQTTUSER",   // Can be omitted if not needed
  "sj102030",   // Can be omitted if not needed
  "MAX7219_DISPLAY",     // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);

void scrollText(void)
{
  static uint32_t prevTime = 0;

  // Is it time to scroll the text?
  if (millis() - prevTime >= SCROLL_DELAY)
  {
    mx.transform(MD_MAX72XX::TSL);  // scroll along - the callback will load all the data
    prevTime = millis();      // starting point for next time
  }
}

void setup()
{
  Serial.begin(115200);
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, INTENSITY);
  mx.setShiftDataInCallback(scrollDataSource);
  mx.setShiftDataOutCallback(scrollDataSink);
  curMessage[0] = newMessage[0] = '\0';
  sprintf(curMessage, "Smart Display");
}

void onConnectionEstablished()
{
  client.subscribe("leddisplay/text", [](const String & payload) {
    SCROLL_DELAY = 75;
    sprintf(curMessage, payload.c_str());
  });

   client.subscribe("leddisplay/time", [](const String & payload) {
    SCROLL_DELAY = 130;
    sprintf(curMessage, payload.c_str());
  });
  
  client.subscribe("leddisplay/intensity", [](const String & payload) {
    mx.control(MD_MAX72XX::INTENSITY, payload.toInt());
  });
  
  client.subscribe("leddisplay/scroll", [](const String & payload) {
    SCROLL_DELAY = payload.toInt();
  });
}

void loop()
{
  client.loop();
  scrollText();
}
