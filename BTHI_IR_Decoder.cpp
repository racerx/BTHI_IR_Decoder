/*
 *
 */
#include "Arduino.h"
#include "BTHI_IR_Decoder.h"

// We use IR Decoder on Arduino pin 2, which is atmega's PD2
#define PIN_ATMEGA PIND
static const int PINBIT_ATMEGA = 2;
static const int PIN_ARDUINO = 2;

BTHI_IR_Decoder::BTHI_IR_Decoder(void) {
}

void BTHI_IR_Decoder::setup(void) {
    pinMode(PIN_ARDUINO, INPUT);
    PCICR  |=  4; // enable PCIE2 which services PCINT18
    PCMSK2 |=  4; // enable PCINT18 --> Pin Change Interrupt of PD2

    // Clear out the frame buffer
    memset(_frame_buffer, 0, sizeof(_frame_buffer));
    _last_timestamp = 0;
    _sample_index = 0;
    _frame_available = 0;

    _k_dead_time = K_DEADTIME_DEFAULT_US;

    // pin debug to check if we are getting here
    _pin = 7;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin,LOW);
    digitalWrite(_pin,HIGH);
    delay(122); // 
    digitalWrite(_pin,LOW);
    

}

// Interrupt handler
void BTHI_IR_Decoder::interrupt() {
    //if (PIN_ATMEGA & (1 << PINBIT_ATMEGA)) return;  // Only consider falling edges
    uint32_t now;
    uint32_t elapsed;
    
    now = micros();
    elapsed = now - _last_timestamp;

    _last_timestamp = now;
  
    // TODO: Handle overflow
    
    if (_sample_index >= sizeof(_frame_buffer)) {
        // XXX: Overflow condition
    }
    
    // Record the sample at the current index
    _frame_buffer[_sample_index].level = 0; // XXX:
    _frame_buffer[_sample_index].duration = elapsed; // XXX:

    _sample_index++;

    if (elapsed > _k_dead_time) {
        // Have to reset the sample index since we're starting over with a new
        // frame
        _sample_index = 0;
        
        // Latch the data and set the flag that new data is available
        memcpy(_latched_frame_buffer, _frame_buffer, sizeof(_frame_buffer));
        _frame_available = 1;
    }

    // TODO: If the _frame_available flag is already set, then we should flag
    // it as a fault and make that available through some API
}

BTHI_IR_Decoder IR_Decoder;

// Register interrupt handler
ISR(PCINT2_vect) {
  IR_Decoder.interrupt();
}

