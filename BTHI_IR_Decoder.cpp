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
    _latched_frame_length = 0;
    _sample_overflows = 0;
	_frame_overruns = 0;

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
    bool pinState;
    
    now = micros();
    elapsed = now - _last_timestamp;

    _last_timestamp = now;
  
    // toggle the pin so we know that we are in the ISR again
    pinState = digitalRead(_pin);
    digitalWrite(_pin,!pinState);
    // TODO: Handle overflow
    
    if (_sample_index >= sizeof(_frame_buffer)) {
        if (_sample_overflows < 0xFFFF) {
            _sample_overflows++;
        }
    }
    
    // Record the sample at the current index
    _frame_buffer[_sample_index].level = pinState; // XXX:
    _frame_buffer[_sample_index].duration = elapsed; // XXX:
    
    _sample_index++;

    if (elapsed > _k_dead_time) {
        // Latch the data and set the flag that new data is available
        // TODO: Theoretically, you only need to copy as many entries as were 
        // valid.
        memcpy(_latched_frame_buffer, _frame_buffer, sizeof(_frame_buffer));

        // Record how many valid samples we took
        _latched_frame_length = _sample_index;

        // If the frame was already waiting, it means that we missed a frame.
        // We call this a "frame overrun"
        if (1 == _frame_available) {
            if (_frame_overruns < 0xFFFF) {
                _frame_overruns++;
            }
        }
        
        _frame_available = 1;
        
        // Have to reset the sample index since we're starting over with a new
        // frame
        _sample_index = 0;
    }
}

uint16_t BTHI_IR_Decoder::getFrameOverrunCount(void) {
    return _frame_overruns;
}

uint16_t BTHI_IR_Decoder::clearFrameOverrunCount(void) {
    uint16_t tmp = _frame_overruns;

    _frame_overruns = 0;

    return tmp;
}

uint16_t BTHI_IR_Decoder::getSampleOverflowCount(void) {
    return _sample_overflows;
}

uint16_t BTHI_IR_Decoder::clearSampleOverflowCount(void) {
    uint16_t tmp = _sample_overflows;

    _sample_overflows = 0;

    return tmp;
}

uint8_t BTHI_IR_Decoder::isFrameAvailable(void) {
    // Check the frame available flag.  Don't need to disable interrupts (I
    // think).
    return _frame_available;
}

/**
 * This function will copy the frame from the internal buffer to the supplied
 * buffer.  If there is no frame available, then no copy is performed and -1
 * is returned.  Otherwise, this function will copy up to buffer_size entries
 * to the supplied pointer.
 *
 * dest_buffer:
 * frame_size: This is the number of entries in the destination buffer, NOT
 * the number of bytes in the dest buffer.
 */
int16_t BTHI_IR_Decoder::copyFrame(ir_decoder_edge_t *dest_buffer, uint16_t buffer_size) {
    if (0 == _frame_available) {
       // TODO: Make E_NOT_OK
       return -1;
    }
    
    // TODO: Have to decide what to do if we know we overran the internal
    // buffer bounds.  I think we should return a special error code.

    // Have to disable interrupts at this point to avoid having the latched
    // frame data overwritten during the copy.
    cli();

    // TODO: consider typdefing uint16 that has to accomodate the largest
    // buffer size.
    // Copy only up to the minimum of the supplied buffer size and the size of
    // the frame in the internal buffer.
    uint16_t num_samples = min(buffer_size, _latched_frame_length);
    memcpy(dest_buffer, _latched_frame_buffer, 
            num_samples * sizeof(_latched_frame_buffer[0]));

    // Put down the flag
    _frame_available = 0;

    // Re-enable interrupts
    sei();
}

BTHI_IR_Decoder IR_Decoder;

// Register interrupt handler
ISR(PCINT2_vect) {
  IR_Decoder.interrupt();
}

