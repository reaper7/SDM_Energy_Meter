#include <SDM.h>                                                           //import SDM220 template library

#define ASCII_ESC 27

char bufout[10];

//SDM<2400, 12, 13> sdm;                                                     //SDM120T	baud, rx pin, tx pin
//SDM<4800, 12, 13> sdm;                                                     //SDM220T	baud, rx pin, tx pin
//SDM<9600, 12, 13> sdm;                                                     //SDM630	baud, rx pin, tx pin
//or without parameters (default from SDM.h will be used): 
SDM<> sdm;

void setup() {
  Serial.begin(115200);                                                         //initialize serial
  sdm.begin();                                                                  //initalize SDM220 communication baudrate
}

void loop() {

  sprintf(bufout,"%c[1;0H",ASCII_ESC);
  Serial.print(bufout);

  Serial.print("Voltage:   ");
  Serial.print(sdm.readVal(SDM220T_VOLTAGE), 2);                                //display voltage
  Serial.println("V");

  Serial.print("Current:   ");
  Serial.print(sdm.readVal(SDM220T_CURRENT), 2);                                //display current  
  Serial.println("A");

  Serial.print("Power:     ");
  Serial.print(sdm.readVal(SDM220T_POWER), 2);                                  //display power
  Serial.println("W");

  Serial.print("Frequency: ");
  Serial.print(sdm.readVal(SDM220T_FREQUENCY), 2);                              //display frequency
  Serial.println("Hz");   


  delay(1000);                                                                  //wait a while before next loop
}
