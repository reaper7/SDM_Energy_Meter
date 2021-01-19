/* Library for reading SDM 72/120/220/230/630 Modbus Energy meters.
*  Reading via Hardware or Software Serial library & rs232<->rs485 converter
*  2016-2020 Reaper7 (tested on wemos d1 mini->ESP8266 with Arduino 1.8.10 & 2.5.2 esp8266 core)
*  crc calculation by Jaime García (https://github.com/peninquen/Modbus-Energy-Monitor-Arduino/)
*/
//------------------------------------------------------------------------------
#include "SDM.h"
//------------------------------------------------------------------------------
#if defined ( USE_HARDWARESERIAL )
#if defined ( ESP8266 )
SDM::SDM(HardwareSerial& serial, long baud, int dere_pin, int config, bool swapuart) : sdmSer(serial) {
  this->_baud = baud;
  this->_dere_pin = dere_pin;
  this->_config = config;
  this->_swapuart = swapuart;
}
#elif defined ( ESP32 )
SDM::SDM(HardwareSerial& serial, long baud, int dere_pin, int config, int8_t rx_pin, int8_t tx_pin) : sdmSer(serial) {
  this->_baud = baud;
  this->_dere_pin = dere_pin;
  this->_config = config;
  this->_rx_pin = rx_pin;
  this->_tx_pin = tx_pin;
}
#else
SDM::SDM(HardwareSerial& serial, long baud, int dere_pin, int config) : sdmSer(serial) {
  this->_baud = baud;
  this->_dere_pin = dere_pin;
  this->_config = config;
}
#endif
#else
#if defined ( ESP8266 ) || defined ( ESP32 )
SDM::SDM(SoftwareSerial& serial, long baud, int dere_pin, int config, int8_t rx_pin, int8_t tx_pin) : sdmSer(serial) {
  this->_baud = baud;
  this->_dere_pin = dere_pin;
  this->_config = config;
  this->_rx_pin = rx_pin;
  this->_tx_pin = tx_pin;
}
#else
SDM::SDM(SoftwareSerial& serial, long baud, int dere_pin) : sdmSer(serial) {
  this->_baud = baud;
  this->_dere_pin = dere_pin;
}
#endif
#endif

SDM::~SDM() {
}

void SDM::begin(void) {
#if defined ( USE_HARDWARESERIAL )
#if defined ( ESP8266 )
  sdmSer.begin(_baud, (SerialConfig)_config);
#elif defined ( ESP32 )
  sdmSer.begin(_baud, _config, _rx_pin, _tx_pin);
#else
  sdmSer.begin(_baud, _config);
#endif
#else
#if defined ( ESP8266 ) || defined ( ESP32 )
  sdmSer.begin(_baud, (SoftwareSerialConfig)_config, _rx_pin, _tx_pin);
#else
  sdmSer.begin(_baud);
#endif
#endif

#if defined ( USE_HARDWARESERIAL ) && defined ( ESP8266 )
  if (_swapuart)
    sdmSer.swap();
#endif
  if (_dere_pin != NOT_A_PIN) {
    pinMode(_dere_pin, OUTPUT);                                                 //set output pin mode for DE/RE pin when used (for control MAX485)
  }
  dereSet(LOW);                                                                 //set init state to receive from SDM -> DE Disable, /RE Enable (for control MAX485)
}

float SDM::readVal(uint16_t reg, uint8_t node) {
  uint16_t temp;
  unsigned long resptime;
  float res = NAN;
  uint8_t readErr = SDM_ERR_NO_ERROR;
  uint8_t readExc = SDM_EXC_NO_EXCEPTION;

  memset(sdmArrI, 0x00, FRAMESIZE);                                             //clear input array
  memset(sdmArrO, 0x00, FRAMESIZE);                                             //clear output array

  sdmArrO[0] = node;
  sdmArrO[1] = SDM_B_02;
  sdmArrO[2] = highByte(reg);
  sdmArrO[3] = lowByte(reg);
  sdmArrO[4] = SDM_B_05;
  sdmArrO[5] = SDM_B_06;
  temp = calculateCRC(sdmArrO, FRAMESIZE - 3);                                  //calculate out crc only from first 6 bytes
  sdmArrO[6] = lowByte(temp);
  sdmArrO[7] = highByte(temp);

#if !defined ( USE_HARDWARESERIAL )
  sdmSer.listen();                                                              //enable softserial rx interrupt
#endif

  flush();                                                                      //read serial if any old data is available

  dereSet(HIGH);                                                                //transmit to SDM  -> DE Enable, /RE Disable (for control MAX485)

  delay(2);                                                                     //fix for issue (nan reading) by sjfaustino: https://github.com/reaper7/SDM_Energy_Meter/issues/7#issuecomment-272111524

  sdmSer.write(sdmArrO, FRAMESIZE - 1);                                         //send 8 bytes

  sdmSer.flush();                                                               //clear out tx buffer

  dereSet(LOW);                                                                 //receive from SDM -> DE Disable, /RE Enable (for control MAX485)

  resptime = millis() + WAITING_TURNAROUND_DELAY;

  while (sdmSer.available() < FRAMESIZE) {
    if (resptime < millis()) {
      readErr = SDM_ERR_TIMEOUT;                                                //err debug (5)
      break;
    }
    yield();
  }

  if (readErr == SDM_ERR_NO_ERROR) {                                            //if no timeout...

    if (sdmSer.available() >= FRAMESIZE) {

      for(int n=0; n<FRAMESIZE; n++) {
        sdmArrI[n] = sdmSer.read();
      }

      if (sdmArrI[0] == node) {

        if (sdmArrI[1] == SDM_B_02 && sdmArrI[2] == SDM_REPLY_BYTE_COUNT) {

          if ((calculateCRC(sdmArrI, FRAMESIZE - 2)) == ((sdmArrI[8] << 8) | sdmArrI[7])) {  //calculate crc from first 7 bytes and compare with received crc (bytes 7 & 8)
            ((uint8_t*)&res)[3]= sdmArrI[3];
            ((uint8_t*)&res)[2]= sdmArrI[4];
            ((uint8_t*)&res)[1]= sdmArrI[5];
            ((uint8_t*)&res)[0]= sdmArrI[6];
          } else {
            readErr = SDM_ERR_CRC_ERROR;                                        //err debug (2)
          }

        } else if (sdmArrI[1] == (SDM_B_02 | 0x80)) {                           //exception response

          if ((calculateCRC(sdmArrI, 3)) == ((sdmArrI[4] << 8) | sdmArrI[3])) { //calculate crc from first 3 bytes and compare with received crc (bytes 3 & 4)
            readExc = sdmArrI[2];
            readErr = SDM_ERR_SDM_EXCEPTION;                                    //err debug (1)
          } else {
            readErr = SDM_ERR_CRC_ERROR;                                        //err debug (2)
          }

        } else {
          readErr = SDM_ERR_WRONG_BYTES;                                        //err debug (3)
        }

      } else {
        readErr = SDM_ERR_WRONG_SLAVE;                                          //err debug (6)
      }

    } else {
      readErr = SDM_ERR_NOT_ENOUGHT_BYTES;                                      //err debug (4)
    }

  }

  flush(RESPONSE_TIMEOUT);                                                      //read serial if any old data is available and wait for RESPONSE_TIMEOUT (in ms)
  
  if (sdmSer.available())                                                       //if serial rx buffer (after RESPONSE_TIMEOUT) still contains data then something spam rs485, check node(s) or increase RESPONSE_TIMEOUT
    readErr = SDM_ERR_TIMEOUT;                                                  //err debug (5) but returned value may be correct

  if (readErr != SDM_ERR_NO_ERROR) {                                            //if error then copy temp error value to global val and increment global error counter
    readingerrcode = readErr;
    if (readExc != SDM_EXC_NO_EXCEPTION)                                        //if sdm exception then copy temp error value to global val
      sdmexceptioncode = readExc;
    readingerrcount++; 
  } else {
    ++readingsuccesscount;
  }

#if !defined ( USE_HARDWARESERIAL )
  sdmSer.stopListening();                                                       //disable softserial rx interrupt
#endif

  return (res);
}

uint8_t SDM::getExcCode(bool _clear) {
  uint8_t _tmp = sdmexceptioncode;
  if (_clear == true)
    clearExcCode();
  return (_tmp);
}

uint8_t SDM::getErrCode(bool _clear) {
  uint8_t _tmp = readingerrcode;
  if (_clear == true)
    clearErrCode();
  return (_tmp);
}

uint32_t SDM::getErrCount(bool _clear) {
  uint32_t _tmp = readingerrcount;
  if (_clear == true)
    clearErrCount();
  return (_tmp);
}

uint32_t SDM::getSuccCount(bool _clear) {
  uint32_t _tmp = readingsuccesscount;
  if (_clear == true)
    clearSuccCount();
  return (_tmp);
}

void SDM::clearExcCode() {
  sdmexceptioncode = SDM_EXC_NO_EXCEPTION;
}

void SDM::clearErrCode() {
  readingerrcode = SDM_ERR_NO_ERROR;
}

void SDM::clearErrCount() {
  readingerrcount = 0;
}

void SDM::clearSuccCount() {
  readingsuccesscount = 0;
}

uint16_t SDM::calculateCRC(uint8_t *array, uint8_t len) {
  uint16_t _crc, _flag;
  _crc = 0xFFFF;
  for (uint8_t i = 0; i < len; i++) {
    _crc ^= (uint16_t)array[i];
    for (uint8_t j = 8; j; j--) {
      _flag = _crc & 0x0001;
      _crc >>= 1;
      if (_flag)
        _crc ^= 0xA001;
    }
  }
  return _crc;
}

void SDM::flush(unsigned long _flushtime) {
  unsigned long flushtime = millis() + _flushtime;
  while (sdmSer.available() || flushtime >= millis()) {
    if (sdmSer.available()) {                                                   //read serial if any old data is available
      sdmSer.read();
      if (flushtime < millis())                                                 //break if serial data is still available
        break;
    }
    delay(1);
  }
}

void SDM::dereSet(bool _state) {
  if (_dere_pin != NOT_A_PIN)
    digitalWrite(_dere_pin, _state);                                            //receive from SDM -> DE Disable, /RE Enable (for control MAX485)
}
