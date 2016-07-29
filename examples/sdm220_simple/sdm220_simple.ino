#include <SDM220_t.h>                                                           //import SDM220 template library

#define ASCII_ESC 27

char bufout[10];

//SDM220<4800, 12, 13> sdm;                                                     //baud, rx pin, tx pin
//or without parameters (default from SDM220_t.h will be used): 
SDM220<> sdm;

void setup() {
  Serial.begin(115200);                                                         //initialize serial
  sdm.begin();                                                                  //initalize SDM220 communication baudrate
}

void loop() {

  sprintf(bufout,"%c[1;0H",ASCII_ESC);
  Serial.print(bufout);

  Serial.print("Voltage:   ");
  Serial.print(sdm.readVal(SDM_VOLTAGE), 2);                                    //display voltage
  Serial.println("V");

  Serial.print("Current:   ");
  Serial.print(sdm.readVal(SDM_CURRENT), 2);                                    //display current  
  Serial.println("A");

  Serial.print("Power:     ");
  Serial.print(sdm.readVal(SDM_POWER), 2);                                      //display power
  Serial.println("W");

  Serial.print("Frequency: ");
  Serial.print(sdm.readVal(SDM_FREQUENCY), 2);                                  //display frequency
  Serial.println("Hz");   


  delay(1000);                                                                  //wait a while before next loop
}
