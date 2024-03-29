/*
 This example works with the Mosiwi Basic learning board.
 
 Learning target:
  1. The relationship between frequency and tone.
	
 Web: http://mosiwi.com
 Wiki: https://mosiwi-wiki.readthedocs.io
 Designer: jalen
 Date：2022-4-6
*/
 //Include library file
#include <MswDT_Button.h>
#include <MswLed.h>

// Define the buzzer using the 10 pin of the M328 PRO(UNO R3).
byte BuzzerPin   = 10; 
// Analog input pin that the potentiometer is attached to       
byte analogInPin = A3;    
   
void setup() {
  // Initialize the 8-segment digital display tube and keys through SPI communication.
  DTB.Start_Init();
  
  // Initialize the control pin of led
  // Parameter1 = ST_CP, Parameter2 = SH_CP, Parameter3 = DS
  Init_LED_IO(8, A1, A0);
}

void loop() {
  // Read the analog value in the potentiometer.
  int analogValue = analogRead(analogInPin);
  // Map analog values to tones.
  byte note = map(analogValue, 0, 1016, 0, 7);
  
  switch(note){
	case 1: tone(BuzzerPin, 523); DTB.DisplayNumber(523); break;    // DO
	case 2: tone(BuzzerPin, 587); DTB.DisplayNumber(587); break;    // RE
	case 3: tone(BuzzerPin, 659); DTB.DisplayNumber(659); break;    // MI
	case 4: tone(BuzzerPin, 698); DTB.DisplayNumber(698); break;    // FA
	case 5: tone(BuzzerPin, 784); DTB.DisplayNumber(784); break;    // SO
	case 6: tone(BuzzerPin, 880); DTB.DisplayNumber(880); break;    // LA
	case 7: tone(BuzzerPin, 988); DTB.DisplayNumber(988); break;    // SI
	default: noTone(BuzzerPin); break;   // Turn off the buzzer.
  }
  if(note != 0){
    SetLedBar(1 << (note-1)); 
  }
  else{
	DTB.DisplayNumber(0);
    SetLedBar(0);
  }
}


