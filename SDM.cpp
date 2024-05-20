/* Library for reading SDM 72/120/220/230/630 Modbus Energy meters.
 *  Reading via Hardware or Software Serial library & rs232<->rs485 converter
 *  2016-2023 Reaper7 (tested on wemos d1 mini->ESP8266 with Arduino 1.8.10 & 2.5.2 esp8266 core)
 *  crc calculation by Jaime García (https://github.com/peninquen/Modbus-Energy-Monitor-Arduino/)
 */
//------------------------------------------------------------------------------
#include "SDM.h"
//------------------------------------------------------------------------------
#if defined(USE_HARDWARESERIAL)
#if defined(ESP8266)
SDM::SDM(HardwareSerial &serial, long baud, int dere_pin, int config, bool swapuart) : sdmSer(serial)
{
  this->_baud = baud;
  this->_dere_pin = dere_pin;
  this->_config = config;
  this->_swapuart = swapuart;
}
#elif defined(ESP32)
SDM::SDM(HardwareSerial &serial, long baud, int dere_pin, int config, int8_t rx_pin, int8_t tx_pin) : sdmSer(serial)
{
  this->_baud = baud;
  this->_dere_pin = dere_pin;
  this->_config = config;
  this->_rx_pin = rx_pin;
  this->_tx_pin = tx_pin;
}
#else
SDM::SDM(HardwareSerial &serial, long baud, int dere_pin, int config) : sdmSer(serial)
{
  this->_baud = baud;
  this->_dere_pin = dere_pin;
  this->_config = config;
}
#endif
#else
#if defined(ESP8266) || defined(ESP32)
SDM::SDM(SoftwareSerial &serial, long baud, int dere_pin, int config, int8_t rx_pin, int8_t tx_pin) : sdmSer(serial)
{
  this->_baud = baud;
  this->_dere_pin = dere_pin;
  this->_config = config;
  this->_rx_pin = rx_pin;
  this->_tx_pin = tx_pin;
}
#else
SDM::SDM(SoftwareSerial &serial, long baud, int dere_pin) : sdmSer(serial)
{
  this->_baud = baud;
  this->_dere_pin = dere_pin;
}
#endif
#endif

SDM::~SDM()
{
}

void SDM::begin(void)
{
#if defined(USE_HARDWARESERIAL)
#if defined(ESP8266)
  sdmSer.begin(_baud, (SerialConfig)_config);
#elif defined(ESP32)
  sdmSer.begin(_baud, _config, _rx_pin, _tx_pin);
#else
  sdmSer.begin(_baud, _config);
#endif
#else
#if defined(ESP8266) || defined(ESP32)
  sdmSer.begin(_baud, (SoftwareSerialConfig)_config, _rx_pin, _tx_pin);
#else
  sdmSer.begin(_baud);
#endif
#endif

#if defined(USE_HARDWARESERIAL) && defined(ESP8266)
  if (_swapuart)
    sdmSer.swap();
#endif
  if (_dere_pin != NOT_A_PIN)
  {
    pinMode(_dere_pin, OUTPUT); // set output pin mode for DE/RE pin when used (for control MAX485)
  }
  dereSet(LOW); // set init state to receive from SDM -> DE Disable, /RE Enable (for control MAX485)
}

float SDM::readVal(uint16_t reg, uint8_t node)
{
  startReadVal(reg, node);

  uint16_t readErr = SDM_ERR_STILL_WAITING;

  while (readErr == SDM_ERR_STILL_WAITING)
  {
    readErr = readValReady(node);
    delay(1);
  }

  if (readErr != SDM_ERR_NO_ERROR)
  { // if error then copy temp error value to global val and increment global error counter
    readingerrcode = readErr;
    readingerrcount++;
  }
  else
  {
    ++readingsuccesscount;
  }

  if (readErr == SDM_ERR_NO_ERROR)
  {
    return decodeFloatValue();
  }

  constexpr float res = NAN;
  return (res);
}

void SDM::startReadVal(uint16_t reg, uint8_t node, uint8_t functionCode)
{
  uint8_t data[] = {
      node,          // Address
      functionCode,  // Modbus function
      highByte(reg), // Start address high byte
      lowByte(reg),  // Start address low byte
      SDM_B_05,      // Number of points high byte
      SDM_B_06,      // Number of points low byte
      0,             // Checksum low byte
      0};            // Checksum high byte

  constexpr size_t messageLength = sizeof(data) / sizeof(data[0]);
  modbusWrite(data, messageLength);
}

uint16_t SDM::readValReady(uint8_t node, uint8_t functionCode)
{
  uint16_t readErr = SDM_ERR_NO_ERROR;
  if (sdmSer.available() < FRAMESIZE && ((millis() - resptime) < mstimeout))
  {
    return SDM_ERR_STILL_WAITING;
  }

  while (sdmSer.available() < FRAMESIZE)
  {
    if ((millis() - resptime) > mstimeout)
    {
      readErr = SDM_ERR_TIMEOUT; // err debug (4)

      if (sdmSer.available() == 5)
      {
        for (int n = 0; n < 5; n++)
        {
          sdmarr[n] = sdmSer.read();
        }
        if (validChecksum(sdmarr, 5))
        {
          readErr = sdmarr[2];
        }
      }
      break;
    }
    delay(1);
  }

  if (readErr == SDM_ERR_NO_ERROR)
  { // if no timeout...

    if (sdmSer.available() >= FRAMESIZE)
    {

      for (int n = 0; n < FRAMESIZE; n++)
      {
        sdmarr[n] = sdmSer.read();
      }

      if (sdmarr[0] == node &&
          sdmarr[1] == functionCode &&
          sdmarr[2] == SDM_REPLY_BYTE_COUNT)
      {
        if (!validChecksum(sdmarr, FRAMESIZE))
        {
          readErr = SDM_ERR_CRC_ERROR; // err debug (1)
        }
      }
      else
      {
        readErr = SDM_ERR_WRONG_BYTES; // err debug (2)
      }
    }
    else
    {
      readErr = SDM_ERR_NOT_ENOUGHT_BYTES; // err debug (3)
    }
  }

  flush(mstimeout); // read serial if any old data is available and wait for RESPONSE_TIMEOUT (in ms)

  if (sdmSer.available())      // if serial rx buffer (after RESPONSE_TIMEOUT) still contains data then something spam rs485, check node(s) or increase RESPONSE_TIMEOUT
    readErr = SDM_ERR_TIMEOUT; // err debug (4) but returned value may be correct

  if (readErr != SDM_ERR_NO_ERROR)
  { // if error then copy temp error value to global val and increment global error counter
    readingerrcode = readErr;
    readingerrcount++;
  }
  else
  {
    ++readingsuccesscount;
  }

#if !defined(USE_HARDWARESERIAL)
//  sdmSer.stopListening();                                                       //disable softserial rx interrupt
#endif
  return readErr;
}

void SDM::enableTransmit()
{
  dereSet(HIGH); // transmit to SDM  -> DE Enable, /RE Disable (for control MAX485)
}

uint8_t SDM::Transmit(uint16_t start, uint16_t end, uint8_t node, uint8_t functionCode)
{
  uint16_t temp;
  uint8_t registers = 2;
  startRegister = start;
  this->node = node;
  this->functionCode = functionCode;
  if (end)
    registers = end - start + 2; // one float = two register = four bytes

  receiveSize = 3 + (registers * 2) + 2; // 3 bytes + (registers * 2) + 2 bytes crc
  uint8_t sdmarr[8] = {node, functionCode, 0, 0, SDM_B_05, registers, 0, 0};

  sdmarr[2] = highByte(start);
  sdmarr[3] = lowByte(start);
  temp = calculateCRC(sdmarr, 6); // calculate out crc only from first 6 bytes
  sdmarr[6] = lowByte(temp);
  sdmarr[7] = highByte(temp);

#if !defined(USE_HARDWARESERIAL)
  sdmSer.listen(); // enable softserial rx interrupt
#endif

  flush();  // read serial if any old data is available

  sdmSer.write(sdmarr, 8);

  return receiveSize;
}

void SDM::disableTransmit()
{
  dereSet(LOW); // receive from SDM -> DE Disable, /RE Enable (for control MAX485)
}

bool SDM::Receive()
{
  return (sdmSer.available() >= receiveSize);
}

uint8_t SDM::available()
{
  return sdmSer.available();
}

uint8_t SDM::Process(void (*callback)(uint16_t reg, float result))
{
  uint8_t *buffer = new uint8_t[receiveSize]();
  uint16_t readErr = SDM_ERR_NO_ERROR;

  for (int n = 0; n < receiveSize; n++)
  {
    buffer[n] = sdmSer.read();
  }

  if (buffer[0] == node && buffer[1] == SDM_B_02 && buffer[2] == (receiveSize - 5))
  {
    uint16_t crc = buffer[receiveSize - 1] << 8 | buffer[receiveSize - 2];
    if (calculateCRC(buffer, receiveSize - 2) == crc)
    {
      for (uint8_t i = 3; i < (receiveSize - 2); i += 4)
      {
        float res;
        ((uint8_t *)&res)[3] = buffer[i];
        ((uint8_t *)&res)[2] = buffer[i + 1];
        ((uint8_t *)&res)[1] = buffer[i + 2];
        ((uint8_t *)&res)[0] = buffer[i + 3];

        callback(startRegister, res);
        startRegister += 2;
      }
    }
    else
    {
      readErr = SDM_ERR_CRC_ERROR;
    }
  }
  else
  {
    readErr = SDM_ERR_WRONG_BYTES;
  }

  delete[] buffer;

  if (readErr == SDM_ERR_NO_ERROR)
    readingsuccesscount++;
  else
    readingerrcount++;

  return readErr;
}

uint8_t SDM::ReceiveError()
{
  for (uint8_t n = 0; n < 5; n++)
  {
    sdmarr[n] = sdmSer.read();
  }
  if (validChecksum(sdmarr, 5))
    return sdmarr[2];
  else
    return SDM_ERR_EXCEPTION;
}

uint8_t SDM::readValues(uint16_t start, uint16_t end, uint8_t node, void (*callback)(uint16_t reg, float result))
{
  uint16_t temp;
  unsigned long resptime;
  uint8_t registers = end - start + 2;            // one float = two register = four bytes
  uint8_t receive_size = 3 + 2 * (registers) + 2; // 3 bytes + 2*(registers) + 2 bytes crc
  uint8_t *buffer = new uint8_t[receive_size]();

  uint8_t sdmarr[FRAMESIZE] = {node, SDM_B_02, 0, 0, SDM_B_05, registers, 0, 0, 0};
  uint16_t readErr = SDM_ERR_NO_ERROR;

  sdmarr[2] = highByte(start);
  sdmarr[3] = lowByte(start);
  temp = calculateCRC(sdmarr, FRAMESIZE - 3); // calculate out crc only from first 6 bytes
  sdmarr[6] = lowByte(temp);
  sdmarr[7] = highByte(temp);

#if !defined(USE_HARDWARESERIAL)
  sdmSer.listen(); // enable softserial rx interrupt
#endif

  flush(); // read serial if any old data is available

  dereSet(HIGH); // transmit to SDM  -> DE Enable, /RE Disable (for control MAX485)

  delay(2); // fix for issue (nan reading) by sjfaustino: https://github.com/reaper7/SDM_Energy_Meter/issues/7#issuecomment-272111524

  sdmSer.write(sdmarr, FRAMESIZE - 1); // send 8 bytes

  sdmSer.flush(); // clear out tx buffer

  dereSet(LOW); // receive from SDM -> DE Disable, /RE Enable (for control MAX485)

  resptime = millis();

  while (sdmSer.available() < receive_size)
  {
    if (millis() - resptime > mstimeout)
    {
      readErr = SDM_ERR_TIMEOUT; // err debug (4)
      break;
    }
    yield();
  }

  if (readErr == SDM_ERR_NO_ERROR)
  { // if no timeout...

    if (sdmSer.available() >= receive_size)
    {

      for (int n = 0; n < receive_size; n++)
      {
        buffer[n] = sdmSer.read();
      }

      if (buffer[0] == node && buffer[1] == SDM_B_02 && buffer[2] == registers * 2)
      {
        uint16_t crc = buffer[receive_size - 1] << 8 | buffer[receive_size - 2];
        if (calculateCRC(buffer, receive_size - 2) == crc)
        {
          for (uint8_t i = 3; i < 3 + registers * 2; i += 4)
          {
            float res;
            ((uint8_t *)&res)[3] = buffer[i];
            ((uint8_t *)&res)[2] = buffer[i + 1];
            ((uint8_t *)&res)[1] = buffer[i + 2];
            ((uint8_t *)&res)[0] = buffer[i + 3];

            callback(start, res);
            start += 2;
          }
        }
        else
        {
          readErr = SDM_ERR_CRC_ERROR; // err debug (1)
        }
      }
      else
      {
        readErr = SDM_ERR_WRONG_BYTES; // err debug (2)
      }
    }
    else
    {
      readErr = SDM_ERR_NOT_ENOUGHT_BYTES; // err debug (3)
    }
  }

  delete[] buffer;

  flush(mstimeout); // read serial if any old data is available and wait for RESPONSE_TIMEOUT (in ms)

  if (sdmSer.available())      // if serial rx buffer (after RESPONSE_TIMEOUT) still contains data then something spam rs485, check node(s) or increase RESPONSE_TIMEOUT
    readErr = SDM_ERR_TIMEOUT; // err debug (4) but returned value may be correct

  if (readErr != SDM_ERR_NO_ERROR)
  { // if error then copy temp error value to global val and increment global error counter
    readingerrcode = readErr;
    readingerrcount++;
  }
  else
  {
    ++readingsuccesscount;
  }

#if !defined(USE_HARDWARESERIAL)
  sdmSer.stopListening(); // disable softserial rx interrupt
#endif

  return (readErr);
}

float SDM::decodeFloatValue() const
{
  if (validChecksum(sdmarr, FRAMESIZE))
  {
    float res{};
    ((uint8_t *)&res)[3] = sdmarr[3];
    ((uint8_t *)&res)[2] = sdmarr[4];
    ((uint8_t *)&res)[1] = sdmarr[5];
    ((uint8_t *)&res)[0] = sdmarr[6];
    return res;
  }
  constexpr float res = NAN;
  return res;
}

float SDM::readHoldingRegister(uint16_t reg, uint8_t node)
{
  startReadVal(reg, node, SDM_READ_HOLDING_REGISTER);

  uint16_t readErr = SDM_ERR_STILL_WAITING;

  while (readErr == SDM_ERR_STILL_WAITING)
  {
    delay(1);
    readErr = readValReady(node, SDM_READ_HOLDING_REGISTER);
  }

  if (readErr != SDM_ERR_NO_ERROR)
  { // if error then copy temp error value to global val and increment global error counter
    readingerrcode = readErr;
    readingerrcount++;
  }
  else
  {
    ++readingsuccesscount;
  }

  if (readErr == SDM_ERR_NO_ERROR)
  {
    return decodeFloatValue();
  }

  constexpr float res = NAN;
  return (res);
}

bool SDM::writeHoldingRegister(float value, uint16_t reg, uint8_t node)
{
  {
    uint8_t data[] = {
        node,                       // Address
        SDM_WRITE_HOLDING_REGISTER, // Function
        highByte(reg),              // Starting Address High
        lowByte(reg),               // Starting Address Low
        SDM_B_05,                   // Number of Registers High
        SDM_B_06,                   // Number of Registers Low
        4,                          // Byte count
        ((uint8_t *)&value)[3],
        ((uint8_t *)&value)[2],
        ((uint8_t *)&value)[1],
        ((uint8_t *)&value)[0],
        0, 0};

    constexpr size_t messageLength = sizeof(data) / sizeof(data[0]);
    modbusWrite(data, messageLength);
  }
  uint16_t readErr = SDM_ERR_STILL_WAITING;
  while (readErr == SDM_ERR_STILL_WAITING)
  {
    delay(1);
    readErr = readValReady(node, SDM_READ_HOLDING_REGISTER);
  }

  if (readErr != SDM_ERR_NO_ERROR)
  { // if error then copy temp error value to global val and increment global error counter
    readingerrcode = readErr;
    readingerrcount++;
  }
  else
  {
    ++readingsuccesscount;
  }

  return readErr == SDM_ERR_NO_ERROR;
}

uint32_t SDM::getSerialNumber(uint8_t node)
{
  uint32_t res{};
  readHoldingRegister(SDM_HOLDING_SERIAL_NUMBER, node);
  //  if (getErrCode() == SDM_ERR_NO_ERROR) {
  for (size_t i = 0; i < 4; ++i)
  {
    res = (res << 8) + sdmarr[3 + i];
  }
  //  }
  return res;
}

uint16_t SDM::getErrCode(bool _clear)
{
  uint16_t _tmp = readingerrcode;
  if (_clear == true)
    clearErrCode();
  return (_tmp);
}

uint32_t SDM::getErrCount(bool _clear)
{
  uint32_t _tmp = readingerrcount;
  if (_clear == true)
    clearErrCount();
  return (_tmp);
}

uint32_t SDM::getSuccCount(bool _clear)
{
  uint32_t _tmp = readingsuccesscount;
  if (_clear == true)
    clearSuccCount();
  return (_tmp);
}

void SDM::clearErrCode()
{
  readingerrcode = SDM_ERR_NO_ERROR;
}

void SDM::clearErrCount()
{
  readingerrcount = 0;
}

void SDM::clearSuccCount()
{
  readingsuccesscount = 0;
}

void SDM::setMsDelay(uint16_t _msdelay)
{
  if (_msdelay < SDM_MIN_DELAY)
    msdelay = SDM_MIN_DELAY;
  else if (_msdelay > SDM_MAX_DELAY)
    msdelay = SDM_MAX_DELAY;
  else
    msdelay = _msdelay;
}

void SDM::setMsTimeout(uint16_t _mstimeout)
{
  if (_mstimeout > SDM_MAX_TIMEOUT)
    mstimeout = SDM_MAX_TIMEOUT;
  else
    mstimeout = _mstimeout;
}

uint16_t SDM::getMsDelay()
{
  return (msdelay);
}

uint16_t SDM::getMsTimeout()
{
  return (mstimeout);
}

uint16_t SDM::calculateCRC(const uint8_t *array, uint8_t len) const
{
  uint16_t _crc, _flag;
  _crc = 0xFFFF;
  for (uint8_t i = 0; i < len; i++)
  {
    _crc ^= (uint16_t)array[i];
    for (uint8_t j = 8; j; j--)
    {
      _flag = _crc & 0x0001;
      _crc >>= 1;
      if (_flag)
        _crc ^= 0xA001;
    }
  }
  return _crc;
}

void SDM::flush(unsigned long _flushtime)
{
  unsigned long flushstart = millis();
  sdmSer.flush();
  int available = sdmSer.available();
  while (available > 0 || ((millis() - flushstart) < _flushtime))
  {
    while (available > 0)
    {
      --available;
      flushstart = millis();
      // read serial if any old data is available
      sdmSer.read();
    }
    delay(1);
    available = sdmSer.available();
  }
}

void SDM::dereSet(bool _state)
{
  if (_dere_pin != NOT_A_PIN)
    digitalWrite(_dere_pin, _state); // receive from SDM -> DE Disable, /RE Enable (for control MAX485)
}

bool SDM::validChecksum(const uint8_t *data, size_t messageLength) const
{
  const uint16_t temp = calculateCRC(data, messageLength - 2); // calculate out crc only from first 6 bytes

  return data[messageLength - 2] == lowByte(temp) &&
         data[messageLength - 1] == highByte(temp);
}

void SDM::modbusWrite(uint8_t *data, size_t messageLength)
{
  const uint16_t temp = calculateCRC(data, messageLength - 2); // calculate out crc only from first 6 bytes

  data[messageLength - 2] = lowByte(temp);
  data[messageLength - 1] = highByte(temp);

#if !defined(USE_HARDWARESERIAL)
  sdmSer.listen(); // enable softserial rx interrupt
#endif

  flush(); // read serial if any old data is available

  if (_dere_pin != NOT_A_PIN)
  {
    dereSet(HIGH); // transmit to SDM  -> DE Enable, /RE Disable (for control MAX485)

    delay(1); // fix for issue (nan reading) by sjfaustino: https://github.com/reaper7/SDM_Energy_Meter/issues/7#issuecomment-272111524

    // Need to wait for all bytes in TX buffer are sent.
    // N.B. flush() on serial port does often only clear the send buffer, not wait till all is sent.
    const unsigned long waitForBytesSent_ms = (messageLength * 11000) / _baud + 1;
    resptime = millis() + waitForBytesSent_ms;
  }

  sdmSer.write(data, messageLength); // send 8 bytes

  if (_dere_pin != NOT_A_PIN)
  {
    const int32_t timeleft = (int32_t)(resptime - millis());
    if (timeleft > 0)
    {
      delay(timeleft); // clear out tx buffer
    }
    dereSet(LOW); // receive from SDM -> DE Disable, /RE Enable (for control MAX485)
    flush();
  }

  resptime = millis();
}
