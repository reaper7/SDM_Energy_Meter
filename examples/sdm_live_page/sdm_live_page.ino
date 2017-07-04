//sdm live page example by reaper7

//#define USE_HARDWARESERIAL
#define READSDMEVERY  1000                                                      //read sdm every 1000ms
#define NBREG   6                                                               //number of sdm registers to read
//#define USE_STATIC_IP

/*  WEMOS D1 Mini                            
                     ______________________________                
                    |   L T L T L T L T L T L T    |
                    |                              |
                 RST|                             1|TX
                  A0|                             3|RX
                  D0|16                           5|D1
                  D5|14                           4|D2
                  D6|12                    10kPUP_0|D3
RX SSer/HSer swap D7|13                LED_10kPUP_2|D4
TX_SSer/HSer swap D8|15                            |GND
                 3V3|__                            |5V
                       |                           |
                       |___________________________|
*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

#include <Ticker.h>

#include <ESPAsyncTCP.h>                                                        // https://github.com/me-no-dev/ESPAsyncTCP
#include <ESPAsyncWebServer.h>                                                  // https://github.com/me-no-dev/ESPAsyncWebServer

#include <TimeLib.h>                                                            // https://github.com/PaulStoffregen/Time
#include <NtpClientLib.h>                                                       // https://github.com/gmag11/NtpClient

#include <SDM.h>                                                                // https://github.com/reaper7/SDM_Energy_Meter

#include "index_page.h"
#include "ota_page.h"
//------------------------------------------------------------------------------
AsyncWebServer server(80);
WiFiClient  client;

#if !defined ( USE_HARDWARESERIAL )                                             // SOFTWARE SERIAL
SDM<9600, 13, 15, NOT_A_PIN> sdm;                                               // baud, rx_pin, tx_pin, de/re_pin(not used in this example)
#else                                                                           // HARDWARE SERIAL
SDM<9600, NOT_A_PIN, true> sdm;                                                 // baud, de/re_pin(not used in this example), swap uart0 from pins 3/1 to 13/15
#endif

Ticker sdmtick;
//------------------------------------------------------------------------------
String devicename = "PWRMETER";

#if defined ( USE_STATIC_IP )
IPAddress ip(192, 168, 0, 130);
IPAddress gateway(192, 168, 0, 254);
IPAddress subnet(255, 255, 255, 0);
#endif

const char* wifi_ssid = "YOUR_SSID";
const char* wifi_password = "YOUR_PASSWORD";

String lastresetreason = "";

volatile bool readsdmflag = true;
volatile bool otalock = false;

unsigned int otaprogress = 0;
//------------------------------------------------------------------------------
enum reglist {
  VOLTAGE = 0,                                                                  //V 
  CURRENT,                                                                      //A 
  POWER,                                                                        //W
  POWER_FACTOR,                                                                 //
  PHASE_ANGLE,                                                                  //DEGREE
  FREQUENCY,                                                                    //Hz
};

typedef volatile struct {
  volatile float regvalarr;
  const uint16_t regarr;
} sdm_struct;

volatile sdm_struct sdmarr[NBREG] = {
  {0.00, SDM220T_VOLTAGE},                                                      //V
  {0.00, SDM220T_CURRENT},                                                      //A
  {0.00, SDM220T_POWER},                                                        //W
  {0.00, SDM220T_POWER_FACTOR},                                                 //PF
  {0.00, SDM220T_PHASE_ANGLE},                                                  //DEGREE
  {0.00, SDM220T_FREQUENCY},                                                    //Hz
};
//------------------------------------------------------------------------------
void xmlrequest(AsyncWebServerRequest *request) {
  String XML = F("<?xml version='1.0'?><xml>");
  if(!otalock) {
    for (int i = 0; i < NBREG; i++) { 
      XML += "<response" + (String)i + ">";  
      XML += String(sdmarr[i].regvalarr,2);
      XML += "</response" + (String)i + ">";
    }
  }
  XML += F("<freeh>");
  XML += String(ESP.getFreeHeap());
  XML += F("</freeh>"); 
  XML += F("<upt>");
  XML += NTP.getUptimeString();
  XML += F("</upt>");
  XML += F("<currt>");
  XML += NTP.getTimeDateString();
  XML += F("</currt>");    
  XML += F("<rst>");
  XML += lastresetreason;
  XML += F("</rst>");
  XML += F("<ota>");
  XML += String(otaprogress);
  XML += F("</ota>");
  XML += F("</xml>");
  request->send(200, "text/xml", XML);
}
//------------------------------------------------------------------------------
void indexrequest(AsyncWebServerRequest *request) {
  request->send_P(200, "text/html", index_page); 
}
//------------------------------------------------------------------------------
void ledOn() {
  digitalWrite(BUILTIN_LED, LOW);  
}
//------------------------------------------------------------------------------
void ledOff() {
  digitalWrite(BUILTIN_LED, HIGH);   
}
//------------------------------------------------------------------------------
void ledSwap() {
  digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));   
}
//------------------------------------------------------------------------------
void otaInit() {
  ArduinoOTA.setHostname(devicename.c_str());

  ArduinoOTA.onStart([]() {
    otalock = true;
    otaprogress = 0;
    ledOn();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    otaprogress = (unsigned int)(progress / (total / 100));
    ledSwap();
  });
  ArduinoOTA.onEnd([]() {
    otaprogress = 100;
    ledOff();
    otalock = false;
  });
  ArduinoOTA.onError([](ota_error_t error) {
    otaprogress = 0;
    ledOff();
    otalock = false;
  });
  ArduinoOTA.begin();
}
//------------------------------------------------------------------------------
void serverInit() {
  server.on("/", HTTP_GET, indexrequest);
  server.on("/xml", HTTP_PUT, xmlrequest); 
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404);
  });
  server.begin();
}
//------------------------------------------------------------------------------
void ntpInit() {
  NTP.begin("pool.ntp.org", 1, true);
  NTP.setInterval(30, 3600);
}
//------------------------------------------------------------------------------
static void wifiInit() {
  WiFi.persistent(false);                                                       // Do not write new connections to FLASH
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

#if defined ( USE_STATIC_IP )
  WiFi.config(ip, gateway, subnet);                                             // Set fixed IP Address
#endif

  while( WiFi.status() != WL_CONNECTED ) {                                      //  Wait for WiFi connection
    ledSwap();
    delay(100);
  }
}
//------------------------------------------------------------------------------
void setsdmreadflag() {
  readsdmflag = true;
}
//------------------------------------------------------------------------------
void sdmRead() {
  float tmpval = NAN;

  for (uint8_t i = 0; i < NBREG; i++) {
    tmpval = sdm.readVal(sdmarr[i].regarr);

    if (isnan(tmpval))
      sdmarr[i].regvalarr = 0.00;
    else
      sdmarr[i].regvalarr = tmpval;

    yield();
  }
}
//------------------------------------------------------------------------------
void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  ledOn();

  lastresetreason = ESP.getResetReason();

  wifiInit();
  otaInit();
  ntpInit();
  serverInit();
  sdm.begin();

  ledOff();
}
//------------------------------------------------------------------------------
void loop() {

  if ((!otalock) && (readsdmflag)) {
    sdmRead();
    readsdmflag = false;
    sdmtick.once_ms((READSDMEVERY), setsdmreadflag);
  }

  ArduinoOTA.handle();
  yield();
}
