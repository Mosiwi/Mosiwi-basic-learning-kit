/*
 This example works with the Mosiwi Basic learning board. 
 
 Learning target:
  1. Infrared Remote
  
 Web: http://mosiwi.com
 Wiki: https://mosiwi-wiki.readthedocs.io
 Designer: jalen
 Date：2022-4-20
*/
// Use timer 1
//#define IR_USE_TIMER1
// Use timer 2
#define IR_USE_TIMER2
#include <MswIR.h>

// Define infrared receiving pins
int RECV_PIN = 4;
// Create an infrared receiver class
MswIR IR(4);

// For the led
const byte R_led = 5;
byte Ok = 0;

void setup(){
  // The infrared receiver function is enabled
  IR.enable(); 
  
  // Initialize LED pins as an output.
  pinMode(R_led, OUTPUT);
  digitalWrite(R_led, LOW);
}

void loop() {		
  // decode and store the decoded data.
  if (IR.decode()) {  
    if(IR.value == 0xff38c7)   // OK button
      Ok = ~Ok;
  }
  digitalWrite(R_led, Ok);
}
