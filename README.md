## Template library for reading SDM120 SDM220 SDM630 Modbus Energy meters. ##

Allows you reading SDM module(s) via:
* Hardware Serial
* Software Serial (library https://github.com/plerup/espsoftwareserial)

and rs232<->rs485 converters:
* with automatic flow direction control (<i>look at hardware_sdm220.jpg</i>)
* with converters with additional pins for flow control, like MAX485
(<i>in this case MAX485 DE and RE pins must be connected together to one of esp pin and this pin must be passed when initializing the library</i>)

**Initializing:**
```
//lib init when Software Serial is used:
#include <SDM.h>
SDM<4800, 13, 15, 12> sdm;  //baudrate, rx pin, tx pin, dere pin(optional for max485)

//lib init when Hardware Serial is used:
#define USE_HARDWARESERIAL
#include <SDM.h>
SDM<4800, 12, false> sdm;  //baudrate, dere pin(optional for max485), swap hw serial pins from 3/1 to 13/15
```
NOTE: <i>when GPIO15 is used (especially for swapped hardware serial):</br>
some converters (like mine) have built-in pullup resistors on TX/RX lines from rs232 side,</br>
connection this type of converters to ESP8266 pin GPIO15 block booting process.</br>
In this case you can replace the pull-up resistor on converter with higher value (100k),</br>
to ensure low level on GPIO15 by built-in in most ESP8266 modules pulldown resistor.</br></i>

**Reading:**</br>
List of available registers for SDM120/220/630:
https://github.com/reaper7/SDM_Energy_Meter/blob/master/SDM.h#L36
```
//reading voltage from SDM with slave address 0x01 (default)
float voltage = sdm.readVal(SDM220T_VOLTAGE);

//reading power from SDM with slave address ID = 0x01
//reading power from SDM with slave address ID = 0x02
//useful with several meters on RS485 line
float power1 = sdm.readVal(SDM220T_POWER, 0x01);
float power2 = sdm.readVal(SDM220T_POWER, 0x02);
```
NOTE: <i>if you reading multiple SDM devices on the same RS485 line,</br>
remember to set the same transmission parameters on each device,</br>
only ID must be different for each SDM device.</i>

---

Tested on Wemos D1 Mini with Arduino IDE 1.8.3-1.9.0b & ESP8266 core 2.3.0-2.4.0

---

:+1: crc calculation by Jaime García (https://github.com/peninquen/Modbus-Energy-Monitor-Arduino/)</br>
:+1: new registers for SDM120 and SDM630 by beireken (https://github.com/beireken/SDM220t)</br>

---

**2018 Reaper7**

[paypal.me/reaper7md](https://www.paypal.me/reaper7md)
