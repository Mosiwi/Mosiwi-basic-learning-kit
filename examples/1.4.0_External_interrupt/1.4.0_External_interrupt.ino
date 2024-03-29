/*
 This example works with the Mosiwi Basic learning board.
 
 Learning target:
	1. External interrupt
	2. Logical operator: !
	3. Variable modifier: const, volatile
	4. Loop statement: while 
	5. Write a function with no return value
	
 Web: http://mosiwi.com
 Wiki: https://mosiwi-wiki.readthedocs.io
 Designer: jalen
 Date：2022-2-12
*/

// Defining character variables. They're used to give names to the pins used: 
const byte ledPin = 5;               // the number of the LED pin(Red LED D5)
const byte interruptPin = 2;         // the number of the pushbutton pin(Button D2)

// variables will change:
volatile char state = 0;           


// Interrupt function with no return value
void blink(void) {
  state = 1 - state;
}


// the setup function runs once when you press reset or power the board
void setup() {
  // Initialize the LED pin as an output:
  pinMode(ledPin, OUTPUT);
  
  //Initialize interrupt pins with a pull resistor.
  pinMode(interruptPin, INPUT_PULLUP);
  
  // Interrupt mode: defines when the interrupt should be triggered. Four constants are predefined as valid values:
  // LOW to trigger the interrupt whenever the pin is low.
  // CHANGE to trigger the interrupt whenever the pin changes value.
  // RISING to trigger when the pin goes from low to high.
  // FALLING for when the pin goes from high to low.
  attachInterrupt(digitalPinToInterrupt(interruptPin), blink, FALLING);
}


// the loop function runs over and over again forever
void loop() {
  //Loop statement
  while(state == 0){
    digitalWrite(ledPin, LOW);
  }

  digitalWrite(ledPin, HIGH);
}



