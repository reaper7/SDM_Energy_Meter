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

#if defined ( USE_HARDWARESERIAL )                                              //for HWSERIAL

#if defined ( ESP8266 )                                                         //for ESP8266
SDM sdm(Serial1, SDM_UART_BAUD, NOT_A_PIN, SERIAL_8N1);                                  //config SDM
#elif defined ( ESP32 )                                                         //for ESP32
SDM sdm(Serial1, SDM_UART_BAUD, NOT_A_PIN, SERIAL_8N1, SDM_RX_PIN, SDM_TX_PIN);          //config SDM
#else                                                                           //for AVR
SDM sdm(Serial1, SDM_UART_BAUD, NOT_A_PIN);                                              //config SDM on Serial1 (if available!)
#endif

#else                                                                           //for SWSERIAL

#include <SoftwareSerial.h>                                                     //import SoftwareSerial library
#if defined ( ESP8266 ) || defined ( ESP32 )                                    //for ESP
SoftwareSerial swSerSDM;                                                        //config SoftwareSerial
SDM sdm(swSerSDM, SDM_UART_BAUD, NOT_A_PIN, SWSERIAL_8N1, SDM_RX_PIN, SDM_TX_PIN);       //config SDM
#else                                                                           //for AVR
SoftwareSerial swSerSDM(SDM_RX_PIN, SDM_TX_PIN);                                //config SoftwareSerial
SDM sdm(swSerSDM, SDM_UART_BAUD, NOT_A_PIN);                                             //config SDM
#endif

#endif

void setup() {
  Serial.begin(115200);                                                         //initialize serial
  sdm.begin();                                                                  //initialize SDM communication
}

void loop() {
  uint8_t error = sdm.readValues(SDM_PHASE_1_VOLTAGE, SDM_PHASE_3_POWER, 0x01, get_result);
  delay(1000);                                                                  //wait a while before next loop
}

// callback function:
void get_result(uint16_t reg, float result)
{
    switch(reg){
      case SDM_PHASE_1_VOLTAGE:
          printf("U1: %.1f V\n", result);
          break;
      case SDM_PHASE_2_VOLTAGE:
          printf("U2: %.1f V\n", result);
          break;
      case SDM_PHASE_3_VOLTAGE:
          printf("U3: %.1f V\n", result);
          break;
      case SDM_PHASE_1_CURRENT:
          printf("I1: %.1f A\n", result);
          break;
      case SDM_PHASE_2_CURRENT:
          printf("I2: %.1f A\n", result);
          break;
      case SDM_PHASE_3_CURRENT:
          printf("I3: %.1f A\n", result);
          break;
      case SDM_PHASE_1_POWER:
          printf("P1: %.1f W\n", result);
          break;
      case SDM_PHASE_2_POWER:
          printf("P2: %.1f W\n", result);
          break;
      case SDM_PHASE_3_POWER:
          printf("P3: %.1f W\n", result);
          break;
      default:
          printf("Register: %d, value: %f\n", reg, result);
          break;
    }
}
