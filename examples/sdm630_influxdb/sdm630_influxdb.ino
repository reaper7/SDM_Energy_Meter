/*  WEMOS D1 Mini
                     ______________________________
                    |   L T L T L T L T L T L T    |
                    |                              |
                 RST|                             1|TX HSer
                  A0|                             3|RX HSer
                  D0|16                           5|D1
                  D5|14                           4|D2
                  D6|12                    10kPUP_0|D3
RX SSer/HSer swap D7|13                LED_10kPUP_2|D4
TX SSer/HSer swap D8|15                            |GND
                 3V3|__                            |5V
                       |                           |
                       |___________________________|
*/

//REMEMBER! uncomment #define USE_HARDWARESERIAL 
//in SDM_Config_User.h file if you want to use hardware uart

#include <SDM.h>                                                                //import SDM library
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <InfluxDbClient.h>           //https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino



// WiFi Parameters
const char* ssid = "SSID";
const char* password = "PASS";
#define HOSTNAME "SDM630"     // Friedly hostname



// InfluxDB v2 server url, e.g. https://eu-central-1-1.aws.cloud2.influxdata.com (Use: InfluxDB UI -> Load Data -> Client Libraries)
// InfluxDB v1 server url "http://192.168.10.2:8086"
#define INFLUXDB_URL "http://INFLUXDB IP:8086"
// InfluxDB v2 server or cloud API authentication token (Use: InfluxDB UI -> Load Data -> Tokens -> <select token>)
//#define INFLUXDB_TOKEN "server token"
// InfluxDB v2 organization id (Use: InfluxDB UI -> Settings -> Profile -> <name under tile> )
//#define INFLUXDB_ORG "org id"
// InfluxDB v2 bucket name (Use: InfluxDB UI -> Load Data -> Buckets)
//#define INFLUXDB_BUCKET "bucket name"
//influxdb v1 database
#define INFLUXDB_DB_NAME "DATABASE"
//influxdb v1 user and pass
#define INFLUXDB_USER "USERNAME"
#define INFLUXDB_PASSWORD "PASSWORD"

// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Examples:
//  Pacific Time:   "PST8PDT"
//  Eastern:        "EST5EDT"
//  Japanesse:      "JST-9"
//  Central Europe: "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
#define WRITE_PRECISION WritePrecision::S
#define MAX_BATCH_SIZE 10
#define WRITE_BUFFER_SIZE 30

// InfluxDB client instance with preconfigured InfluxCloud certificate
//InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_DB_NAME);

// Number for loops to sync time using NTP
int iterations = 0;

const int dere_pin = 2;

//REMEMBER TO SET BAUD TO WHAT YOUR METER SET SET TO 

#if defined ( USE_HARDWARESERIAL )                                              //for HWSERIAL

#if defined ( ESP8266 )                                                         //for ESP8266
SDM sdm(Serial, 38400, dere_pin, SERIAL_8N1);                                  //config SDM
#else                                                                           //for AVR
SDM sdm(Serial, 38400, dere_pin);                                              //config SDM on Serial1 (if available!)
#endif

#else                                                                           //for SWSERIAL

#include <SoftwareSerial.h>                                                     //import SoftwareSerial library
#if defined ( ESP8266 )                                                        //for ESP
SoftwareSerial swSerSDM;                                                        //config SoftwareSerial
SDM sdm(swSerSDM, 9600, dere_pin, SWSERIAL_8N1, SDM_RX_PIN, SDM_TX_PIN);       //config SDM
#else                                                                           //for AVR
SoftwareSerial swSerSDM(SDM_RX_PIN, SDM_TX_PIN);                                //config SoftwareSerial
SDM sdm(swSerSDM, 9600, dere_pin);                                             //config SDM
#endif

#endif //#if defined ( USE_HARDWARESERIAL )

#define READSDMEVERY  1000                                                      //read sdm every 2000ms
#define NBREG   23    // SET TO the number of parameters in sdm_struct sdmarr[NBREG] and maximum 40 


typedef struct {
  float regvalarr;
  const uint16_t regarr;
  const String regtext;
} sdm_struct;

//uncomment and change NBREG to get more parameters
sdm_struct sdmarr[NBREG] = {
  {0.00, SDM630_VOLTAGE1,"VoltageL1"},                                                      //V
  {0.00, SDM630_VOLTAGE2,"VoltageL2"},                                                      //V
  {0.00, SDM630_VOLTAGE3,"VoltageL3"},                                                      //V
//  {0.00, SDM630_VOLTAGE_AVERAGE,"VoltageAVG"},                                              //V       
  {0.00, SDM630_CURRENT1,"CurrentL1"},                                                      //A
  {0.00, SDM630_CURRENT2,"CurrentL2"},                                                      //A
  {0.00, SDM630_CURRENT3,"CurrentL3"},                                                      //A
  {0.00, SDM630_CURRENTSUM,"CurrentSUM"},                                                   //A
//  {0.00, SDM630_CURRENT_AVERAGE,"CurrentAVG"},                                              //A       
  {0.00, SDM630_POWER1,"PowerL1"},                                                          //W
  {0.00, SDM630_POWER2,"PowerL2"},                                                          //W
  {0.00, SDM630_POWER3,"PowerL3"},                                                          //W
  {0.00, SDM630_POWERTOTAL,"PowerSUM"},                                                     //W
//  {0.00, SDM630_POWER_FACTOR1,"PFL1"},                                                      //PF      
//  {0.00, SDM630_POWER_FACTOR2,"PFL2"},                                                      //PF      
//  {0.00, SDM630_POWER_FACTOR3,"PFL3"},                                                      //PF      
  {0.00, SDM630_POWER_FACTOR_TOTAL,"PFTOTAL"},                                              //PF
//  {0.00, SDM630_PHASE_ANGLE1,"PAL1"},                                                       //Degrees 
//  {0.00, SDM630_PHASE_ANGLE2,"PAL2"},                                                       //Degrees 
//  {0.00, SDM630_PHASE_ANGLE3,"PAL3"},                                                       //Degrees 
//  {0.00, SDM630_PHASE_ANGLE_TOTAL,"PATOTAL"},                                               //Degrees 
  {0.00, SDM630_FREQUENCY,"FREQUENCY"},                                                     //Hz
  {0.00, SDM630_IMPORT_ACTIVE_ENERGY,"ImportEnergi"},                                       //Wh
//  {0.00, SDM630_EXPORT_ACTIVE_ENERGY,"ExportEnergi"},                                       //Wh      
//  {0.00, SDM630_TOTAL_SYSTEM_POWER_DEMAND,"TotalSystemPower"},                              //W       
//  {0.00, SDM630_MAXIMUM_TOTAL_SYSTEM_POWER,"TotalMaxSystemPower"},                          //W       
  {0.00, SDM630_TOTAL_ACTIVE_ENERGY,"TotalEnergi"},                                         //Wh
  {0.00, SDM630_PHASE_12_VOLTAGE,"VoltageL1L2"},                                            //V
  {0.00, SDM630_PHASE_23_VOLTAGE,"VoltageL2L3"},                                            //V
  {0.00, SDM630_PHASE_31_VOLTAGE,"VoltageL3L1"},                                            //V
//  {0.00, SDM630_PHASE_VOLTAGE_AVERAGE,"VoltagePhaseAVG"},                                   //V       
  {0.00, SDM630_VOLT_AMPS_REACTIVE_TOTAL,"ReactivePowerSUM"},                               //VAr       
  {0.00, SDM630_VOLT_AMPS_TOTAL,"ApparentPowerSUM"},                                        //VA      
  {0.00, SDM630_IMPORT1,"ImportL1"},                                                        //kWh  
  {0.00, SDM630_IMPORT2,"ImportL2"},                                                        //kWh 
  {0.00, SDM630_IMPORT3,"ImportL3"},                                                        //kWh 
};

unsigned long readtime;
time_t ntpLastUpdate;
int ntpSyncTime = 3600;
bool read_done = false;


void setup() {
  //Serial.begin(115200);                                                         //initialize serial
  sdm.begin();                                                                  //initialize SDM communication

  // Setup wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); 
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    //Serial.println("Connecting...");
  }
  
  // ***************************************************************************
  // Setup: MDNS responder
  // ***************************************************************************
  MDNS.begin(HOSTNAME);
  //Serial.print("Hostname: ");
  //Serial.print(HOSTNAME);

  ArduinoOTA.begin();


  // Set InfluxDB 1 authentication params
  client.setConnectionParamsV1(INFLUXDB_URL, INFLUXDB_DB_NAME, INFLUXDB_USER, INFLUXDB_PASSWORD);
  
  // Sync time for certificate validation
  timeSync();

  // Check server connection
  if (client.validateConnection()) {
    //Serial.print("Connected to InfluxDB: ");
    //Serial.println(client.getServerUrl());
  } else {
    //Serial.print("InfluxDB connection failed: ");
    //Serial.println(client.getLastErrorMessage());
  }

  //Enable messages batching and retry buffer
  client.setWriteOptions(WRITE_PRECISION, MAX_BATCH_SIZE, WRITE_BUFFER_SIZE);


  
}

void loop() {
  // Sync time for batching once per hour
  if(time(nullptr)-ntpLastUpdate > ntpSyncTime) {//if (iterations++ >= 360) {
    timeSync();
    iterations = 0;
  }

  if (millis() - readtime >= READSDMEVERY) {
    sdmRead();
    readtime = millis();
  }

  if(read_done){
//put data to influx in buffer
    time_t tnow = time(nullptr);
    for (int i = 0; i < NBREG; i++) {
      Point powerMeter("SDM630");
      //powerMeter.addTag("device", "Main");
      powerMeter.addTag("Type", sdmarr[i].regtext );
      //powerMeter.addTag("channel", String(WiFi.channel(i)));
      //powerMeter.addTag("open", String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN));
      powerMeter.addField("value", sdmarr[i].regvalarr);
      powerMeter.setTime(tnow);  //set the time

      // Print what are we exactly writing
      //Serial.print("Writing: ");
      //Serial.println(powerMeter.toLineProtocol());

      // Write point into buffer - low priority measures
      client.writePoint(powerMeter);
 
    }





  // End of the iteration - force write of all the values into InfluxDB as single transaction
  //Serial.println("Flushing data into InfluxDB");
  if (!client.flushBuffer()) {
    //Serial.print("InfluxDB flush failed: ");
    //Serial.println(client.getLastErrorMessage());
    //Serial.print("Full buffer: ");
    //Serial.println(client.isBufferFull() ? "Yes" : "No");
  }
  

  

  
    for(int i = 0; i < NBREG; i++) {
      //Serial.print(sdmarr[i].regtext);
      //Serial.print(" = ");
      //Serial.println(sdmarr[i].regvalarr);
    
    }
    read_done = false;
    
    
  }

  ArduinoOTA.handle();
  yield();
  /*Serial.println("Wait 10s");
  delay(10000);*/
}


//------------------------------------------------------------------------------

void timeSync() {
  // Synchronize UTC time with NTP servers
  // Accurate time is necessary for certificate validaton and writing in batches
  configTime(0, 0, "pool.ntp.org", "time.nis.gov");
  // Set timezone
  setenv("TZ", TZ_INFO, 1);

  // Wait till time is synced
  //Serial.print("Syncing time");
  int i = 0;
  while (time(nullptr) < 1000000000ul && i < 100) {
    //Serial.print(".");
    delay(100);
    i++;
  }
  //Serial.println();

  // Show time
  time_t tnow = time(nullptr);
  ntpLastUpdate = time(nullptr);
  //Serial.print("Synchronized time: ");
  //Serial.println(String(ctime(&tnow)));
}



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
  read_done = true;
}
