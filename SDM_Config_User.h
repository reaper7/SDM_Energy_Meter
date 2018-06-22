/* Library for reading SDM 120/220/230/630 Modbus Energy meters.
*  Reading via Hardware or Software Serial library & rs232<->rs485 converter
*  2016-2018 Reaper7 (tested on wemos d1 mini->ESP8266 with Arduino 1.9.0-beta & 2.4.1 esp8266 core)
*  crc calculation by Jaime García (https://github.com/peninquen/Modbus-Energy-Monitor-Arduino/)
*/
//USER CONFIG:
//------------------------------------------------------------------------------
#ifdef USE_HARDWARESERIAL
  #undef USE_HARDWARESERIAL                                                     //undefine USE_HARDWARESERIAL from SDM_Config.h
#endif
#define USE_HARDWARESERIAL                                                      //define USE_HARDWARESERIAL
//------------------------------------------------------------------------------
#ifdef SDM_UART_BAUD
  #undef SDM_UART_BAUD                                                          //undefine baudrate from SDM_Config.h
#endif
#define SDM_UART_BAUD                       9600                                //define new baudrate
//------------------------------------------------------------------------------
#ifdef DERE_PIN
  #undef DERE_PIN                                                               //undefine DERE_PIN from SDM_Config.h
#endif
#define DERE_PIN                            NOT_A_PIN                           //define new DERE_PIN for control MAX485 DE/RE lines (connect DE & /RE together to this pin)
//------------------------------------------------------------------------------
#ifdef USE_HARDWARESERIAL

  #ifdef SDM_UART_CONFIG
    #undef SDM_UART_CONFIG                                                      //undefine SDM_UART_CONFIG from SDM_Config.h
  #endif
  #define SDM_UART_CONFIG                   SERIAL_8N1                          //define new SDM_UART_CONFIG

  #ifdef SWAPHWSERIAL
    #undef SWAPHWSERIAL                                                         //undefine SWAPHWSERIALG from SDM_Config.h
  #endif
  #define SWAPHWSERIAL                      0                                   //define new SWAPHWSERIAL, if true(1) then swap uart pins from 3/1 to 13/15 (only ESP8266)

#endif
//------------------------------------------------------------------------------
#ifdef MAX_MILLIS_TO_WAIT
  #undef MAX_MILLIS_TO_WAIT                                                     //undefine MAX_MILLIS_TO_WAIT from SDM_Config.h
#endif
#define MAX_MILLIS_TO_WAIT                  500                                 //define new MAX_MILLIS_TO_WAIT to wait for response from SDM
//------------------------------------------------------------------------------