/*---------------------------------------------------------------------------
 * Streaming Infrared Decoder Library
 * 
 * This library uses the input capture functionality of Timer 1 on the
 * atmega328 to do processing of the input waveform from an IR receiver such
 * as the Vishay TSOP1140.
 *
 * The use of Timer 1's input capture has the advantages of:
 *   - overall lower CPU load vs a timer-driven polling approach
 *   - more reliable than using pulseIn()
 *   - uses Timer 1's built-in noise canceller
 *
 * Disadvantages:
 *   - Restricted to Pin 8
 *   - One cannot use PWM channels that are driven by Timer 1
 *
 * Another design goal was to allow for both streaming and buffered decoding
 * approaches.  To accomodate this, there is a class responsible for
 * interfacing with the input capture hardware, but a delegate is handed each
 * segment duration.  Once there, it can either decode it on the fly or buffer
 * it.  A buffered implementation is provided in IR_BufferingStreamDecoder.
 *
 * A streaming approach:
 *    - Can be more complex to implement
 *    - Uses much less RAM
 *    - Would likely be designed specifically for a single IR protocol
 *
 * A buffered approach:
 *    - Uses much more RAM
 *    - May miss frames while processing the last received frame
 *    - Can be made to evaluate the waveform against many known protocols
 */
#include <Arduino.h>
#include "BTHI_IR_Decoder.h"

// We have to instantiate this because we're wiring it manually to the timer1
// capture and overflow ISRs. The instance needs to be valid.
IR_HwInterface IR_InputCaptureInterface;

/**
 * Setup routine. This routine must be called during your projects setup()
 * function. It sets up Timer1 to do input capture and chooses an initial
 * level to capture on based on your polarity setting. It also sets the
 * specified pin to be an INPUT.
 *
 * NOTE: After this call, your PWM's using Timer1 won't work anymore.
 *
 * stream_decoder: This delegate will be fed edges and end of frame events.
 * pin: Must be pin 8 on the Uno. TODO: what about others?
 * polarity: This determines what the initial edge the input capture unit
 *    should trigger on. If your receiver is nominally HIGH when there is no
 *    IR activity, then you should choose IR_POLARITY_HIGH. If it's nominally
 *    LOW, then choose IR_POLARITY_LOW. If you just don't know yet, you can
 *    also choose IR_POLARITY_AUTO. This option looks at the level on the line
 *    and tries to figure it out for you. Probably ok.. but not foolproof.
 *
 */
void IR_HwInterface::setup(IR_StreamDecoder *stream_decoder, 
        uint8_t pin, ir_polarity_t polarity) {
    _pin = pin;
    _decoder = stream_decoder;
    
    // Set Initial Timer value
    TCNT1 = 0;
    
    // Put timer 1 into "Normal" mode for input capture
    TCCR1A = 0;

    if (IR_POLARITY_AUTO == polarity) {
        // Have to set the pin mode to input early if it's auto to read the
        // level.  We'll call this again later, but that shouldn't have any
        // side effects.
        pinMode(pin, INPUT);
    
        if (0 == digitalRead(_pin)) {
            // First edge is rising
            TCCR1B |= (1 << ICES1);
        } else {
            // First edge is falling
            TCCR1B &= ~(1 << ICES1);
        }
    } else if (IR_POLARITY_LOW == polarity) {
        // First edge is rising
        TCCR1B |= (1 << ICES1);
    } else {
        // First edge is falling
        TCCR1B &= ~(1 << ICES1);
    }

    // Enable input capture interrupts only
    TIMSK1 = (1 << ICIE1);

    // Noise cancellation on and prescaler /8
    TCCR1B = (1 << ICNC1) | (1 << CS11);

    pinMode(pin, INPUT);
}

IR_HwInterface::IR_HwInterface() {
}

void IR_HwInterface::overflowInterrupt() {
    // No more overflow interrupt until it is re-enabled in the capture
    // interrupt.
    TIMSK1 &= ~(1<<TOIE1);
    
    if (_decoder) {
        _decoder->endDecode();
    }
}

// Interrupt handler
void IR_HwInterface::captureInterrupt() {
    uint16_t elapsed;
    uint8_t level;
    
    // Reset TCNT1
    TCNT1 = 0;

    elapsed = ICR1;
    
    // Start listening for overflow as well as the input capture interrupt
    // Make sure to clear the overflow flag, otherwise it will trigger
    // immediately, giving us a premature end of frame
    TIFR1 = (1 << TOV1);
    TIMSK1 = (1<<ICIE1) | (1<<TOIE1);

    level = digitalRead(_pin);
    
    if (0 == level) {
        // Next edge is rising
        TCCR1B |= (1 << ICES1);
    } else {
        // Next edge is falling
        TCCR1B &= ~(1 << ICES1);
    }

    // Pass it on to the decode layer
    if (_decoder) {
        _decoder->decodeEdge(elapsed);
    }
}

IR_BufferingStreamDecoder::IR_BufferingStreamDecoder(void) {
	ir_segment_t *_segments = NULL;
	_max_segments = 0;
	_count = 0;
	_segment_overflows = 0;
	_frame_complete = 0;
    _first_edge = 1;
}

void IR_BufferingStreamDecoder::debugPrintFrame(void) {
    Serial.print("Max Segments: ");
    Serial.println(_max_segments);
    Serial.print("Segment Count: ");
    Serial.println(_count);
    Serial.print("Segment Overflow: ");
    Serial.println(_segment_overflows);

    for (uint8_t i = 0; i < _count; i++) {
        Serial.print(i);
        Serial.print(": ");
        Serial.println(_segments[i].duration);
    }
}

void IR_BufferingStreamDecoder::endDecode(void) {
    if (_count > 0) {
        _frame_complete = 1;
    }
}

void IR_BufferingStreamDecoder::decodeEdge(uint16_t duration) {
    // Don't overrun the frame until reset()
    if (0 != _frame_complete) {
        return;
    }
    
    // First edge is ignored because it doesn't complete a segment.
    // That is, it has no meaningful duration.
    if (1 == _first_edge) {
        _first_edge = 0;
        return;
    }

    // Record the sample at the current index
    if (_count >= _max_segments) {
        if (_segment_overflows < (uint8_t)0xFF) {
            _segment_overflows++;
        }

        return;
    }

    _segments[_count++].duration = duration;
}

void IR_BufferingStreamDecoder::setSegmentBuffer(ir_segment_t *segments, 
        uint8_t num_segments) {
    cli();
    _segments = segments;
    _max_segments = num_segments;
    _count = 0;
    _frame_complete = 0;
    _segment_overflows = 0;
    _first_edge = 1;
    sei();
}

void IR_BufferingStreamDecoder::receiveNextFrame(void) {
    cli();
    _count = 0;
    _frame_complete = 0;
    _segment_overflows = 0;
    _first_edge = 1;
    sei();
}

uint8_t IR_BufferingStreamDecoder::isDone(void) {
    return _frame_complete;
}

uint8_t IR_BufferingStreamDecoder::getSegmentCount(void) {
    return _count;
}

uint8_t IR_BufferingStreamDecoder::getSegmentOverflowCount(void) {
    return _segment_overflows;
}

uint8_t IR_BufferingStreamDecoder::clearSegmentOverflowCount(void) {
    uint8_t tmp = _segment_overflows;
    _segment_overflows = 0;
    return tmp;
}

// Register interrupt handler
ISR(TIMER1_CAPT_vect) {
    IR_InputCaptureInterface.captureInterrupt();
}

ISR(TIMER1_OVF_vect) {
    IR_InputCaptureInterface.overflowInterrupt();
}

