/* Library for reading SDM 72/120/220/230/630 Modbus Energy meters.
*  Reading via Hardware or Software Serial library & rs232<->rs485 converter
*  2016-2022 Reaper7 (tested on wemos d1 mini->ESP8266 with Arduino 1.8.10 & 2.5.2 esp8266 core)
*  crc calculation by Jaime García (https://github.com/peninquen/Modbus-Energy-Monitor-Arduino/)
*/
//------------------------------------------------------------------------------
#include "SDM.h"
//------------------------------------------------------------------------------
#if defined ( USE_HARDWARESERIAL ) || defined ( ESP32 )
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
#if defined ( ESP8266 )
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
#if defined ( USE_HARDWARESERIAL ) || defined ( ESP32 )
#if defined ( ESP8266 )
  sdmSer.begin(_baud, (SerialConfig)_config);
#elif defined ( ESP32 )
  sdmSer.begin(_baud, _config, _rx_pin, _tx_pin);
#else
  sdmSer.begin(_baud, _config);
#endif
#else
#if defined ( ESP8266 )
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
  uint8_t sdmarr[FRAMESIZE] = {node, SDM_B_02, 0, 0, SDM_B_05, SDM_B_06, 0, 0, 0};
  float res = NAN;
  uint16_t readErr = SDM_ERR_NO_ERROR;

  sdmarr[2] = highByte(reg);
  sdmarr[3] = lowByte(reg);

  temp = calculateCRC(sdmarr, FRAMESIZE - 3);                                   //calculate out crc only from first 6 bytes

  sdmarr[6] = lowByte(temp);
  sdmarr[7] = highByte(temp);

#if !defined ( USE_HARDWARESERIAL ) && !defined ( ESP32 )
  sdmSer.listen();                                                              //enable softserial rx interrupt
#endif

  flush();                                                                      //read serial if any old data is available

  dereSet(HIGH);                                                                //transmit to SDM  -> DE Enable, /RE Disable (for control MAX485)

  delay(2);                                                                     //fix for issue (nan reading) by sjfaustino: https://github.com/reaper7/SDM_Energy_Meter/issues/7#issuecomment-272111524

  sdmSer.write(sdmarr, FRAMESIZE - 1);                                          //send 8 bytes

  sdmSer.flush();                                                               //clear out tx buffer

  dereSet(LOW);                                                                 //receive from SDM -> DE Disable, /RE Enable (for control MAX485)

  resptime = millis();

  while (sdmSer.available() < FRAMESIZE) {
    if (millis() - resptime > msturnaround) {
      readErr = SDM_ERR_TIMEOUT;                                                //err debug (4)
      break;
    }
    yield();
  }

  if (readErr == SDM_ERR_NO_ERROR) {                                            //if no timeout...

    if (sdmSer.available() >= FRAMESIZE) {

      for(int n=0; n<FRAMESIZE; n++) {
        sdmarr[n] = sdmSer.read();
      }

      if (sdmarr[0] == node && sdmarr[1] == SDM_B_02 && sdmarr[2] == SDM_REPLY_BYTE_COUNT) {

        if ((calculateCRC(sdmarr, FRAMESIZE - 2)) == ((sdmarr[8] << 8) | sdmarr[7])) {  //calculate crc from first 7 bytes and compare with received crc (bytes 7 & 8)
          ((uint8_t*)&res)[3]= sdmarr[3];
          ((uint8_t*)&res)[2]= sdmarr[4];
          ((uint8_t*)&res)[1]= sdmarr[5];
          ((uint8_t*)&res)[0]= sdmarr[6];
        } else {
          readErr = SDM_ERR_CRC_ERROR;                                          //err debug (1)
        }

      } else {
        readErr = SDM_ERR_WRONG_BYTES;                                          //err debug (2)
      }

    } else {
      readErr = SDM_ERR_NOT_ENOUGHT_BYTES;                                      //err debug (3)
    }

  }

  flush(mstimeout);                                                             //read serial if any old data is available and wait for RESPONSE_TIMEOUT (in ms)
  
  if (sdmSer.available())                                                       //if serial rx buffer (after RESPONSE_TIMEOUT) still contains data then something spam rs485, check node(s) or increase RESPONSE_TIMEOUT
    readErr = SDM_ERR_TIMEOUT;                                                  //err debug (4) but returned value may be correct

  if (readErr != SDM_ERR_NO_ERROR) {                                            //if error then copy temp error value to global val and increment global error counter
    readingerrcode = readErr;
    readingerrcount++; 
  } else {
    ++readingsuccesscount;
  }

#if !defined ( USE_HARDWARESERIAL ) && !defined ( ESP32 )
  sdmSer.stopListening();                                                       //disable softserial rx interrupt
#endif

  return (res);
}

uint16_t SDM::getErrCode(bool _clear) {
  uint16_t _tmp = readingerrcode;
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

void SDM::clearErrCode() {
  readingerrcode = SDM_ERR_NO_ERROR;
}

void SDM::clearErrCount() {
  readingerrcount = 0;
}

void SDM::clearSuccCount() {
  readingsuccesscount = 0;
}

void SDM::setMsTurnaround(uint16_t _msturnaround) {
  if (_msturnaround < SDM_MIN_DELAY)
    msturnaround = SDM_MIN_DELAY;
  else if (_msturnaround > SDM_MAX_DELAY)
    msturnaround = SDM_MAX_DELAY;
  else
    msturnaround = _msturnaround; 
}

void SDM::setMsTimeout(uint16_t _mstimeout) {
  if (_mstimeout < SDM_MIN_DELAY)
    mstimeout = SDM_MIN_DELAY;
  else if (_mstimeout > SDM_MAX_DELAY)
    mstimeout = SDM_MAX_DELAY;
  else
    mstimeout = _mstimeout; 
}

uint16_t SDM::getMsTurnaround() {
  return (msturnaround);
}

uint16_t SDM::getMsTimeout() {
  return (mstimeout);
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
  unsigned long flushstart = millis();
  while (sdmSer.available() || (millis() - flushstart < _flushtime)) {
    if (sdmSer.available())                                                     //read serial if any old data is available
      sdmSer.read();
    delay(1);
  }
}

void SDM::dereSet(bool _state) {
  if (_dere_pin != NOT_A_PIN)
    digitalWrite(_dere_pin, _state);                                            //receive from SDM -> DE Disable, /RE Enable (for control MAX485)
}
