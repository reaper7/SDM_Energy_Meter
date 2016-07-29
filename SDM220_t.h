/* Template library for reading SDM220 Modbus Energy meter.
*  Reading via Software Serial library & rs232<->rs485 converter
*  2016 Reaper7 (tested on wemos d1 mini->ESP8266 with Arduino 1.6.9 & 2.3.0 esp8266 core)
*  crc calculation by Jaime García (https://github.com/peninquen/Modbus-Energy-Monitor-Arduino/)
*/

#ifndef SDM220_T_h
#define SDM220_T_h
//------------------------------------------------------------------------------
#include <Arduino.h>
#include <SoftwareSerial.h>
//------------------------------------------------------------------------------
#define SDM_BAUD                    4800                                        //baudrate
#define MAX_MILLIS_TO_WAIT          1000                                        //max time to wait for responce from SDM
#define SDM_READ_EVERY              1000                                        //read SDM every ms

#define SDMSER_RX                   12                                          //RX-D6(wemos)-12
#define SDMSER_TX                   13                                          //TX-D7(wemos)-13

#define FRAMESIZE                   9                                           //size of out/in array
//------------------------------------------------------------------------------
#define SDM_B_01                    0x01                                        //BYTE 1
#define SDM_B_02                    0x04                                        //BYTE 2
                                                                                //BYTES 3 & 4 (BELOW)
#define SDM_VOLTAGE                 0x0000                                      //V
#define SDM_CURRENT                 0x0006                                      //A
#define SDM_POWER                   0x000C                                      //W
#define SDM_ACTIVE_APPARENT_POWER   0x0012                                      //VA
#define SDM_REACTIVE_APPARENT_POWER 0x0018                                      //VAR
#define SDM_POWER_FACTOR            0x001E                                      //
#define SDM_PHASE_ANGLE             0x0024                                      //DEGREE
#define SDM_FREQUENCY               0x0046                                      //Hz
#define SDM_IMPORT_ACTIVE_ENERGY    0x0048                                      //Wh
#define SDM_EXPORT_ACTIVE_ENERGY    0x004A                                      //Wh
#define SDM_IMPORT_REACTIVE_ENERGY  0x004C                                      //VARh
#define SDM_EXPORT_REACTIVE_ENERGY  0x004E                                      //VARh
#define SDM_TOTAL_ACTIVE_ENERGY     0x0156                                      //Wh
#define SDM_TOTAL_REACTIVE_ENERGY   0x0158                                      //VARh

#define SDM_B_05                    0x00                                        //BYTE 5
#define SDM_B_06                    0x02                                        //BYTE 6
//------------------------------------------------------------------------------
template <long _speed = SDM_BAUD, int _rx_pin = SDMSER_RX, int _tx_pin = SDMSER_TX>
struct SDM220 {

  SoftwareSerial sdmSer = SoftwareSerial(_rx_pin, _tx_pin, false, 32);

  private:

    uint16_t calculateCRC(uint8_t *array, uint8_t num) {
      uint16_t temp, flag;
      temp = 0xFFFF;
      for (uint8_t i = 0; i < num; i++) {
        temp = temp ^ array[i];
        for (uint8_t j = 8; j; j--) {
          flag = temp & 0x0001;
          temp >>= 1;
          if (flag)
            temp ^= 0xA001;
        }
      }
      return temp;
    };

  public:

    void begin() {
      sdmSer.begin(_speed);
    };

    float readVal(uint16_t reg) {
      uint16_t temp;
      unsigned long resptime;
      uint8_t sdmarr[FRAMESIZE] = {SDM_B_01, SDM_B_02, 0, 0, SDM_B_05, SDM_B_06, 0, 0, 0};
      float res = -9999.99;

      sdmarr[2] = highByte(reg);
      sdmarr[3] = lowByte(reg);

      temp = calculateCRC(sdmarr, FRAMESIZE - 3);                               //calculate out crc only from first 6 bytes

      sdmarr[6] = lowByte(temp);
      sdmarr[7] = highByte(temp);
      
      sdmSer.write(sdmarr, FRAMESIZE - 1);                                      //send 8 bytes

      resptime = millis();

      while ( (sdmSer.available() < FRAMESIZE) && ((millis() - resptime) < MAX_MILLIS_TO_WAIT) ) {      
        delay(1);
      }

      if(sdmSer.available() == FRAMESIZE) {
        for(int n=0; n<FRAMESIZE; n++) {
          sdmarr[n] = sdmSer.read();
        }

        if (sdmarr[0] == SDM_B_01 && sdmarr[1] == SDM_B_02 && sdmarr[2] == SDM_B_02) {
          if ((calculateCRC(sdmarr, FRAMESIZE - 2)) == ((sdmarr[8] << 8) | sdmarr[7])) {  //calculate crc from first 7 bytes and compare with received crc (bytes 7 & 8)
            ((uint8_t*)&res)[3]= sdmarr[3];
            ((uint8_t*)&res)[2]= sdmarr[4];
            ((uint8_t*)&res)[1]= sdmarr[5];
            ((uint8_t*)&res)[0]= sdmarr[6];      
          }
        }
      }
      sdmSer.flush();
      return (res);   
    };

};
#endif