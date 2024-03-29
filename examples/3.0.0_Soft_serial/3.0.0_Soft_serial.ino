/*
 * Project: Software serial
 * Function: TTL uart communication
 * Serial port communication protocol: 
   https://en.wikipedia.org/wiki/Serial_port
   https://www.analog.com/en/analog-dialogue/articles/uart-a-hardware-communication-protocol.html
 * Wiki: https://mosiwi-wiki.readthedocs.io
 * Web: http://mosiwi.com
 * Engineer: Jalen
 * date: 2022-4-15
 */
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <Arduino.h>
#include <util/delay_basic.h>
#include <inttypes.h>

#define _SS_MAX_RX_BUFF 64 // RX buffer size

#ifndef GCC_VERSION
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

class SoftwareSerial {
private:
  // per object data
  uint8_t _receivePin;
  uint8_t _receiveBitMask;
  volatile uint8_t *_receivePortRegister;
  uint8_t _transmitBitMask;
  volatile uint8_t *_transmitPortRegister;
  volatile uint8_t *_pcint_maskreg;
  uint8_t _pcint_maskvalue;

  // Expressed as 4-cycle delays (must never be 0!)
  uint16_t _rx_delay_centering;
  uint16_t _rx_delay_intrabit;
  uint16_t _rx_delay_stopbit;
  uint16_t _tx_delay;
  // Occupies one bit of a byte, which can only be 0 or 1.
  uint16_t _buffer_overflow:1;

  // static data
  static uint8_t _receive_buffer[_SS_MAX_RX_BUFF]; 
  static volatile uint8_t _receive_buffer_tail;
  static volatile uint8_t _receive_buffer_head;
  static SoftwareSerial *active_object;

  // private methods
  inline void recv() __attribute__((__always_inline__));
  uint8_t rx_pin_read();
  void setTX(uint8_t transmitPin);
  void setRX(uint8_t receivePin);
  inline void setRxIntMsk(bool enable) __attribute__((__always_inline__));

  // Return num - sub, or 1 if the result would be < 1
  static uint16_t subtract_cap(uint16_t num, uint16_t sub);
  // private static method for timing
  static inline void tunedDelay(uint16_t delay);

public:
  // public methods
  SoftwareSerial(uint8_t receivePin, uint8_t transmitPin);
  ~SoftwareSerial();
  void begin(long speed);
  bool listen();
  bool isListening() { return this == active_object; }
  bool stopListening();
  bool overflow() { bool ret = _buffer_overflow; if (ret) _buffer_overflow = false; return ret; }

  virtual size_t write(uint8_t byte);
  virtual int read();
  virtual int available();
  // Overloading the bool operator to return a bool using if(SoftwareSerial.Object).
  operator bool() { return true; }

  // public only for easy access by interrupt handlers
  static inline void handle_interrupt() __attribute__((__always_inline__));
};

// Initializes a static variable of the class.
SoftwareSerial *SoftwareSerial::active_object = 0;
uint8_t SoftwareSerial::_receive_buffer[_SS_MAX_RX_BUFF]; 
volatile uint8_t SoftwareSerial::_receive_buffer_tail = 0;
volatile uint8_t SoftwareSerial::_receive_buffer_head = 0;

//
// Private methods
//

// static 
inline void SoftwareSerial::tunedDelay(uint16_t delay) { 
  _delay_loop_2(delay); // Delay 4*delay machine cycles
}

// This function sets the current object as the "listening"
// one and returns true if it replaces another 
bool SoftwareSerial::listen(){
  if (!_rx_delay_stopbit)
    return false;

  if (active_object != this){
    if (active_object)
      active_object->stopListening();

    _buffer_overflow = false;
    _receive_buffer_head = _receive_buffer_tail = 0;
    active_object = this;

    setRxIntMsk(true);
    return true;
  }
  return false;
}

// Stop listening. Returns true if we were actually listening.
bool SoftwareSerial::stopListening(){
  if (active_object == this){
    setRxIntMsk(false);
    active_object = NULL;
    return true;
  }
  return false;
}

// The receive routine called by the interrupt handler
void SoftwareSerial::recv(){
#if GCC_VERSION < 40302
// Work-around for avr-gcc 4.3.0 OSX version bug
// Preserve the registers that the compiler misses
// (courtesy of Arduino forum user *etracer*)
  asm volatile(
    "push r18 \n\t"
    "push r19 \n\t"
    "push r20 \n\t"
    "push r21 \n\t"
    "push r22 \n\t"
    "push r23 \n\t"
    "push r26 \n\t"
    "push r27 \n\t"
    ::);
#endif  

  uint8_t d = 0;
  // If RX line is high, then we don't see any start bit
  // so interrupt is probably not for us
  if (!rx_pin_read()){
    // Disable further interrupts during reception, this prevents
    // triggering another interrupt directly after we return, which can
    // cause problems at higher baudrates.
    setRxIntMsk(false);

    // Wait approximately 1/2 of a bit width to "center" the sample
    tunedDelay(_rx_delay_centering);

    // Read each of the 8 bits
    for (uint8_t i=8; i > 0; --i){
      tunedDelay(_rx_delay_intrabit);
      d >>= 1;
      if (rx_pin_read())
        d |= 0x80;
    }

    // if buffer full, set the overflow flag and return
    uint8_t next = (_receive_buffer_tail + 1) % _SS_MAX_RX_BUFF;
    if (next != _receive_buffer_head){
      // save new data in buffer: tail points to where byte goes
      _receive_buffer[_receive_buffer_tail] = d; // save new byte
      _receive_buffer_tail = next;
    } else {
      _buffer_overflow = true;
    }
    // skip the stop bit
    tunedDelay(_rx_delay_stopbit);

    // Re-enable interrupts when we're sure to be inside the stop bit
    setRxIntMsk(true);
  }

#if GCC_VERSION < 40302
// Work-around for avr-gcc 4.3.0 OSX version bug
// Restore the registers that the compiler misses
  asm volatile(
    "pop r27 \n\t"
    "pop r26 \n\t"
    "pop r23 \n\t"
    "pop r22 \n\t"
    "pop r21 \n\t"
    "pop r20 \n\t"
    "pop r19 \n\t"
    "pop r18 \n\t"
    ::);
#endif
}

uint8_t SoftwareSerial::rx_pin_read(){
  return *_receivePortRegister & _receiveBitMask;
}

// Interrupt handling
// static 
inline void SoftwareSerial::handle_interrupt(){
  if (active_object){
    active_object->recv();
  }
}

#if defined(PCINT0_vect)
ISR(PCINT0_vect){
  SoftwareSerial::handle_interrupt();
}
#endif

#if defined(PCINT1_vect)
ISR(PCINT1_vect, ISR_ALIASOF(PCINT0_vect));
#endif

#if defined(PCINT2_vect)
ISR(PCINT2_vect, ISR_ALIASOF(PCINT0_vect));
#endif

#if defined(PCINT3_vect)
ISR(PCINT3_vect, ISR_ALIASOF(PCINT0_vect));
#endif

// Constructor
SoftwareSerial::SoftwareSerial(uint8_t receivePin, uint8_t transmitPin) : 
  // Class private variable initialization
  _rx_delay_centering(0),
  _rx_delay_intrabit(0),
  _rx_delay_stopbit(0),
  _tx_delay(0),
  _buffer_overflow(false){
  setTX(transmitPin);
  setRX(receivePin);
}

// Destructor
SoftwareSerial::~SoftwareSerial(){
  stopListening();
}

void SoftwareSerial::setTX(uint8_t tx){
  // First write, then set output. If we do this the other way around,
  // the pin would be output low for a short while before switching to
  // output high. Now, it is input with pullup for a short while, which is fine. 
  digitalWrite(tx, HIGH);
  pinMode(tx, OUTPUT);
  _transmitBitMask = digitalPinToBitMask(tx);
  uint8_t port = digitalPinToPort(tx);
  _transmitPortRegister = portOutputRegister(port);
}

void SoftwareSerial::setRX(uint8_t rx){
  pinMode(rx, INPUT);
  digitalWrite(rx, HIGH);  // pullup for normal logic!
  _receivePin = rx;
  _receiveBitMask = digitalPinToBitMask(rx);
  uint8_t port = digitalPinToPort(rx);
  _receivePortRegister = portInputRegister(port);
}

uint16_t SoftwareSerial::subtract_cap(uint16_t num, uint16_t sub) {
  if (num > sub)
    return num - sub;
  else
    return 1;
}

//
// Public methods
//

void SoftwareSerial::begin(long speed){
  _rx_delay_centering = _rx_delay_intrabit = _rx_delay_stopbit = _tx_delay = 0;

  // Precalculate the various delays, in number of 4-cycle delays
  uint16_t bit_delay = (F_CPU / speed) / 4;

  // 12 (gcc 4.8.2) or 13 (gcc 4.3.2) cycles from start bit to first bit,
  // 15 (gcc 4.8.2) or 16 (gcc 4.3.2) cycles between bits,
  // 12 (gcc 4.8.2) or 14 (gcc 4.3.2) cycles from last bit to stop bit
  // These are all close enough to just use 15 cycles, since the inter-bit
  // timings are the most critical (deviations stack 8 times)
  _tx_delay = subtract_cap(bit_delay, 15 / 4);

  // Only setup rx when we have a valid PCINT for this pin
  if (digitalPinToPCICR((int8_t)_receivePin)) {
    #if GCC_VERSION > 40800
    // Timings counted from gcc 4.8.2 output. This works up to 115200 on
    // 16Mhz and 57600 on 8Mhz.
    //
    // When the start bit occurs, there are 3 or 4 cycles before the
    // interrupt flag is set, 4 cycles before the PC is set to the right
    // interrupt vector address and the old PC is pushed on the stack,
    // and then 75 cycles of instructions (including the RJMP in the
    // ISR vector table) until the first delay. After the delay, there
    // are 17 more cycles until the pin value is read (excluding the
    // delay in the loop).
    // We want to have a total delay of 1.5 bit time. Inside the loop,
    // we already wait for 1 bit time - 23 cycles, so here we wait for
    // 0.5 bit time - (71 + 18 - 22) cycles.
    _rx_delay_centering = subtract_cap(bit_delay / 2, (4 + 4 + 75 + 17 - 23) / 4);

    // There are 23 cycles in each loop iteration (excluding the delay)
    _rx_delay_intrabit = subtract_cap(bit_delay, 23 / 4);

    // There are 37 cycles from the last bit read to the start of
    // stopbit delay and 11 cycles from the delay until the interrupt
    // mask is enabled again (which _must_ happen during the stopbit).
    // This delay aims at 3/4 of a bit time, meaning the end of the
    // delay will be at 1/4th of the stopbit. This allows some extra
    // time for ISR cleanup, which makes 115200 baud at 16Mhz work more reliably
    _rx_delay_stopbit = subtract_cap(bit_delay * 3 / 4, (37 + 11) / 4);
    #else // Timings counted from gcc 4.3.2 output
    // Note that this code is a _lot_ slower, mostly due to bad register
    // allocation choices of gcc. This works up to 57600 on 16Mhz and 38400 on 8Mhz.
    _rx_delay_centering = subtract_cap(bit_delay / 2, (4 + 4 + 97 + 29 - 11) / 4);
    _rx_delay_intrabit = subtract_cap(bit_delay, 11 / 4);
    _rx_delay_stopbit = subtract_cap(bit_delay * 3 / 4, (44 + 17) / 4);
    #endif


    // Enable the PCINT for the entire port here, but never disable it
    // (others might also need it, so we disable the interrupt by using
    // the per-pin PCMSK register).
    *digitalPinToPCICR((int8_t)_receivePin) |= _BV(digitalPinToPCICRbit(_receivePin));
    // Precalculate the pcint mask register and value, so setRxIntMask
    // can be used inside the ISR without costing too much time.
    _pcint_maskreg = digitalPinToPCMSK(_receivePin);
    _pcint_maskvalue = _BV(digitalPinToPCMSKbit(_receivePin));

    tunedDelay(_tx_delay); // if we were low this establishes the end
  }
  listen();
}

void SoftwareSerial::setRxIntMsk(bool enable){
    if (enable)
      *_pcint_maskreg |= _pcint_maskvalue;
    else
      *_pcint_maskreg &= ~_pcint_maskvalue;
}

// Read data from buffer
int SoftwareSerial::read(){
  if (!isListening())
    return -1;

  // Empty buffer?
  if (_receive_buffer_head == _receive_buffer_tail)
    return -1;

  // Read from "head"
  uint8_t d = _receive_buffer[_receive_buffer_head]; // grab next byte
  _receive_buffer_head = (_receive_buffer_head + 1) % _SS_MAX_RX_BUFF;
  return d;
}

int SoftwareSerial::available(){
  if (!isListening())
    return 0;

  return (_receive_buffer_tail + _SS_MAX_RX_BUFF - _receive_buffer_head) % _SS_MAX_RX_BUFF;
}

size_t SoftwareSerial::write(uint8_t b){
  if (_tx_delay == 0) {
    //setWriteError();
    return 0;
  }

  // By declaring these as local variables, the compiler will put them
  // in registers _before_ disabling interrupts and entering the critical 
  // timing sections below, which makes it a lot easier to verify the cycle timings
  volatile uint8_t *reg = _transmitPortRegister;
  uint8_t reg_mask = _transmitBitMask;
  uint8_t inv_mask = ~_transmitBitMask;
  uint8_t oldSREG = SREG;
  uint16_t delay = _tx_delay;
  // turn off interrupts for a clean txmit
  cli();  

  // Write the start bit
  *reg &= inv_mask;
  tunedDelay(delay);

  // Write each of the 8 bits
  for (uint8_t i = 8; i > 0; --i){
    if (b & 1)          // choose bit
      *reg |= reg_mask; // send 1
    else
      *reg &= inv_mask; // send 0

    tunedDelay(delay);
    b >>= 1;
  }

  // restore pin to natural state
  *reg |= reg_mask;
  // turn interrupts back on
  SREG = oldSREG; 
  tunedDelay(_tx_delay);
  
  return 1;
}

// Define a class object.
SoftwareSerial mySerial(0, 1); // RX, TX
// bps: 1200 --- 19200
int bps = 9600;
int delayT = bps > 1000000 ? 1 : 1000000/bps;
////////////////////////////////////////////
void setup() {                
  mySerial.begin(bps);
}

////////////////////////////////////////////
void loop() { 
  uint8_t dat[64];
  uint8_t cache_num = 0;
  while (mySerial.available() > 0) {
    dat[cache_num] = mySerial.read();
    delayMicroseconds(delayT);
    cache_num ++;
  }
  if(cache_num > 0){
    for(uint8_t i=0; i<cache_num; i++){
      mySerial.write(dat[i]);
    }
  }
}

