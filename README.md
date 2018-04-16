Template library for reading SDM120 / SDM220 / SDM630 Modbus Energy meters.

Reading via Hardware Serial or Software Serial (library https://github.com/plerup/espsoftwareserial)<br>
and rs232<->rs485 converter with automatic flow direction control (look at hardware_sdm220.jpg)<br>
or with converters with additional pins for flow control, like MAX485.<br>
(In this case MAX485 DE and RE pins must be connected together to one of esp pin and this pin must be passed when initializing the library)

Initializing:
```
//lib init when Software Serial is used:
#include <SDM.h>
SDM<4800, 13, 15, 12> sdm;  //baudrate, rx pin, tx pin, dere pin(optional for max485)

//lib init when Hardware Serial is used:
#define USE_HARDWARESERIAL
#include <SDM.h>
SDM<4800, 12, false> sdm;  //baudrate, dere pin(optional for max485), swap hw serial pins from 3/1 to 13/15
```
note for GPIO15 (especially for swapped hardware serial):
some converters (like mine) have built-in pullup resistors on TX/RX lines from rs232 side,
connection this type of converters to ESP8266 pin GPIO15 block booting process.
In this case you can replace the pull-up resistor on converter with higher value (100k),
to ensure low level on GPIO15 by built-in in most ESP8266 modules pulldown resistor.

Reading:
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

Tested on wemos d1 mini->ESP8266 with Arduino 1.8.3 & 2.3.0 esp8266 core

crc calculation by Jaime García (https://github.com/peninquen/Modbus-Energy-Monitor-Arduino/)

UPDATE:<br>
- (31.10.2016) new registers for SDM120 and SDM630 by beireken (https://github.com/beireken/SDM220t)

2016 - 2017 Reaper7

[paypal.me/reaper7md](https://www.paypal.me/reaper7md)
