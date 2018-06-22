/* Library for reading SDM 120/220/230/630 Modbus Energy meters.
*  Reading via Hardware or Software Serial library & rs232<->rs485 converter
*  2016-2018 Reaper7 (tested on wemos d1 mini->ESP8266 with Arduino 1.9.0-beta & 2.4.1 esp8266 core)
*  crc calculation by Jaime García (https://github.com/peninquen/Modbus-Energy-Monitor-Arduino/)
*/
//------------------------------------------------------------------------------
//DEFAULT CONFIG:
//------------------------------------------------------------------------------
//#define USE_HARDWARESERIAL                                                    //define USE_HARDWARESERIAL for hardwareserial
// or
#ifdef USE_HARDWARESERIAL
  #undef USE_HARDWARESERIAL                                                     //undefine USE_HARDWARESERIAL for softwareserial
#endif
//------------------------------------------------------------------------------
#define SDM_UART_BAUD                       4800                                //baudrate

#define DERE_PIN                            NOT_A_PIN                           //digital pin for control MAX485 DE/RE lines (connect DE & /RE together to this pin)

#ifdef USE_HARDWARESERIAL
  #define SDM_UART_CONFIG                   SERIAL_8N1                          //uart config
  #define SWAPHWSERIAL                      0                                   //when hwserial used, then swap uart pins from 3/1 to 13/15
#endif

#define MAX_MILLIS_TO_WAIT                  500                                 //max time to wait for response from SDM
//------------------------------------------------------------------------------
