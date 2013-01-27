/*---------------------------------------------------------------------------
 * Streaming Infrared Decoder Library
 * 
 * This library uses the input capture functionality of Timer 1 on the
 * atmega328 to do processing of the input waveform from an IR receiver such
 * as the Vishay TSOP1140.
 *
 * As edges of the signal come in from the IR receiver IC, we are notified and
 * note the duration between the edge and the previous edge of the frame. This
 * gives you enough information to reconstruct the binary data from the IR
 * emitter.
 * 
 * Here is some example data for a Samsung remote when Vol+ button is pressed.
 * It gives you an idea of the kind of data you'll be processing.
 *
 * 0: 9067      [First half of start of frame. 4.5ms]
 * 1: 8818      [Second half of start of frame. 4.5ms]
 * 2: 1252      [Always comes first in a bit. 0.56ms]
 * 3: 3273      [Bit is a 1 because the second half is ~1.6ms although
 *               technically, the spec says it should be 2.25ms]
 * 4: 1208      
 * 5: 3273      [Bit is a 1]
 * 6: 1208
 * 7: 3272      [Bit is a 1]
 * 8: 1207
 * 9: 1025      [Bit is a 0]
 * 10: 1207
 * 11: 1025     [Bit is a 0]
 * 12: 1207
 * 13: 1024     [Bit is a 0]
 * 14: 1207
 * 15: 1016     [Bit is a 0]
 * 16: 1207
 * ... Continued up to 66 edges.
 *
 * Notes on use of Timer 1:
 *  The use of Timer 1's input capture has the advantages of:
 *   - overall lower CPU load vs a timer-driven polling approach
 *   - more reliable than using pulseIn()
 *   - uses Timer 1's built-in noise canceller
 *
 *  Disadvantages:
 *   - Restricted to Pin 8
 *   - One cannot use PWM channels that are driven by Timer 1
 *
 * Notes on streaming vs buffering:
 *  Another design goal was to allow for both streaming and buffered decoding
 *  approaches.  To accomodate this, there is a class responsible for
 *  interfacing with the input capture hardware, but a delegate is handed each
 *  segment duration.  Once there, it can either decode it on the fly or buffer
 *  it.  A buffered implementation is provided in IR_BufferingStreamDecoder.
 *
 *  A streaming approach:
 *    - Can be more complex to implement
 *    - Uses much less RAM
 *    - Would likely be designed specifically for a single IR protocol
 *
 *  A buffered approach:
 *    - Uses much more RAM
 *    - May miss frames while processing the last received frame
 *    - Can be made to evaluate the waveform against many known protocols
 *
 *
 * Resources:
 *  IR Theory Generally - http://www.sbprojects.com/knowledge/ir/nec.php
 *  More Protocol - http://www.techdesign.be/projects/011/011_waves.htm
 *
 */
#include <Arduino.h>
#include "BTHI_IR_Decoder.h"

#define IR_DEFAULT_IC_PIN   8

/* We have to instantiate this because we're wiring it manually to the timer1
 * capture and overflow ISRs. The instance needs to be valid.
 */
IR_HwInterface IR_InputCaptureInterface;

/**
 * Constructor for the Hardware Interface. Does nothing since we use setup()
 * to provide the run-time parameters
 *
 * Parameters: None
 *
 * Returns: Nothing
 */
IR_HwInterface::IR_HwInterface() {
	_pin = IR_DEFAULT_IC_PIN;
	_decoder = NULL;
}

/**
 * Setup routine. This routine must be called during your projects setup()
 * function. It sets up Timer1 to do input capture and chooses an initial
 * level to capture on based on your polarity setting. It also sets the
 * specified pin to be an INPUT.
 *
 * NOTE: After this call, your PWM's using Timer1 won't work anymore.
 *
 * Parameters:
 *      stream_decoder: This delegate will be fed edges and end of frame 
 *          events.
 *      pin: Must be pin 8 on the Uno. TODO: what about others?
 *      polarity: This determines what the initial edge the input capture 
 *          unit should trigger on. If your receiver is nominally HIGH when 
 *          there is no IR activity, then you should choose IR_POLARITY_HIGH. 
 *          If it's nominally LOW, then choose IR_POLARITY_LOW. If you just 
 *          don't know yet, you can also choose IR_POLARITY_AUTO. This option 
 *          looks at the level on the line and tries to figure it out for you. 
 *          Probably ok.. but not foolproof.
 * 
 * Parameters: None
 * 
 * Return: Nothing
 */
void IR_HwInterface::setup(IR_StreamDecoder *stream_decoder, 
        uint8_t pin, ir_polarity_t polarity) {
    // Make sure interrupts are disabled.  We're not entirely sure of what was
    // done before calling our setup.
    cli();
    
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
    // This gives a tick period of 50us
    // 1 / (16000000 Hz /8) = 50us
    TCCR1B = (1 << ICNC1) | (1 << CS11);

    // The pin needs to be an input for input capture to work
    pinMode(pin, INPUT);

    // Re-enable interrupts
    sei();
}

/**
 * This function implements the overflow interrupt ISR for Timer 1. This
 * interrupt fires when the TCNT overflows from 0xFFFF to 0x0000. For us, it
 * means that it's been 65536 * 50us = 0.032768s (32.768 ms) since the last
 * edge. This is enough time to be certain that the transmitter has finished a
 * frame and from what I've seen, also enough time that it doesn't catch the
 * edge of a subsequent frame.
 *
 * NOTE: Should be called from the TIMER1_OVF_vect ISR
 * 
 * Parameters: None
 * 
 * Return: Nothing
 */
void IR_HwInterface::overflowInterrupt(void) {
    // No more overflow interrupt until it is re-enabled in the capture
    // interrupt.
    TIMSK1 &= ~(1<<TOIE1);
    
    // Tell our decoding delegate that it's the end of the frame
    if (NULL != _decoder) {
        _decoder->endOfFrameEvent();
    }
}

/**
 * This function implements the edge capture interrupt ISR for Timer 1. This
 * interrupt fires when an edge occurs on our input pin in the direction
 * matching the ICES1 field of the TCCR1B register (rising or falling).
 *
 * At the time this interrupt fires, Timer 1 has already cleared the interrupt
 * flag, and TCNT has been latched into the ICR1 register (and continues to
 * run). We need to reset TCNT back to 0 as quickly as possible so that ICR1
 * always contains the number of 50us ticks since the last edge. This means
 * we don't need to do any now-then math to figure out elapsed time.
 *
 * Next, we'll enable the overflow interrupt (see
 * IR_HwInterface::overflowInterrupt) which will fire if we don't get another
 * edge before 0xFFFF ticks.
 *
 * NOTE: Should be called from the TIMER1_CAPT_vect ISR
 */
void IR_HwInterface::captureInterrupt(void) {
    uint16_t elapsed;
    uint8_t level;
    
    // Reset TCNT1
    TCNT1 = 0;

    // ICR1 contains TCNT1 value at the time of the edge event
    elapsed = ICR1;
    
    // Start listening for overflow as well as the input capture interrupt
    // Make sure to clear the overflow flag, otherwise it will trigger
    // immediately, giving us a premature end of frame
    TIFR1 = (1 << TOV1);
    TIMSK1 = (1 << ICIE1) | (1 << TOIE1);

    // Figure out what the current level is by looking at what condition we
    // had used to capture the edge.  If it was rising, the level is obviously
    // HIGH and we need to now look for a falling edge (and vice versa).
    level = (TCCR1B & (1 << ICES1)) != 0 ? HIGH : LOW;

    if (LOW == level) {
        // Next edge is rising
        TCCR1B |= (1 << ICES1);
    } else {
        // Next edge is falling
        TCCR1B &= ~(1 << ICES1);
    }

    // Pass it on to the decode delegate
    if (NULL != _decoder) {
        _decoder->edgeEvent(elapsed);
    }
}

/**
 * Constructor for the IR_BufferingStreamDecoder, a Decoder delegate
 * implementation that buffers all of the waveform segments
 * it sees for later analysis.
 *
 * Does nothing but initialize internal state.
 * 
 * Parameters: None
 * 
 * Return: Nothing
 */
IR_BufferingStreamDecoder::IR_BufferingStreamDecoder(void) {
	ir_segment_t *_segments = NULL;
	_max_segments = 0;
	_count = 0;
	_segment_overflows = 0;
	_frame_complete = 0;
    _first_edge = 1;
}

/**
 * Debugging routine that prints the contents of the current frame. Be
 * careful, because reception could be in progress at the time. We recommend
 * calling it like this:
 *
 *  if (decoder.isDone()) {
 *      decoder.debugPrintFrame();
 *      decoder.receiveNextFrame();
 *  }
 *
 * Parameters: None
 * 
 * Return: Nothing
 */
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

/**
 * IR_StreamDecoder implementation of endOfFrameEvent. We use an internal flag
 * to record that we're received the end of a frame. We'll set that flag here
 * if we've seen more than zero edges in the frame.
 *
 * After the HwImplementation calls this method, we don't allow any changes to
 * be made to the received frame to give you time to process it. When done
 * processing, calling the readyForNextFrame() method puts the flag down so we
 * can receive the next frame.
 *
 * Parameters: None
 * 
 * Return: Nothing
 */
void IR_BufferingStreamDecoder::endOfFrameEvent(void) {
    if (_count > 0) {
        _frame_complete = 1;
    }
}

/**
 * IR_StreamDecoder implementation of edgeEvent. We look at the duration
 * provided and we store it in the next slot in our buffer. In cases where the
 * buffer isn't large enough, we increment our count of segment overflows (see 
 * getSegmentOverflowCount()) and drop the duration. This is a clue to you
 * that whatever remote control you're using requires you to use a larger
 * segment buffer.
 *
 * NOTE: We also drop the first segment as it isn't helpful. It represents a
 * random duration between the last TCNT1 overflow and the first edge of the
 * waveform--not helpful.
 *
 * Parameters:
 *      duration: number of TCNT1 ticks that have transpired since the last 
 *          edge event.
 * 
 * Return: Nothing
 */
void IR_BufferingStreamDecoder::edgeEvent(uint16_t duration) {
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

/**
 * This function provides the buffering stream decoder with the buffer that it
 * should use to hold the edges. Originally, this object took care of that
 * for you, but at the expense of malloc.
 *
 * Example:
 *
 *  IR_BufferingStreamDecoder decoder;
 *  ir_segment_t g_segment_buffer[72];
 *
 *  void setup() {
 *      decoder.setSegmentBuffer(g_segment_buffer, 72);
 *  }
 *
 * Parameters:
 *      segments:   A pointer to an ir_segment_t array. Can be NULL only if
 *          num_segments is zero.
 *      num_segments: The number of segments in the segments array. If you
 *          don't get these to be the same, you could risk writing segments
 *          past the end of your buffer and the consequences would be unknown.
 *
 * Return: Nothing
 *
 * TODO: If this class gets pulled out as an example rather than being part of
 * the base library, then we're free to use #defines to correctly set the size
 * of the buffer internally and we don't need to ask the user. This wouldn't
 * work, though in cases where you wanted multiple decoders with different
 * size buffers.
 */
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

/**
 * Tells the buffering decoder delegate that it's ok to start receiving the
 * next frame. You need to call this when you're done with the previous frame.
 * That is, once isDone() returns 1, you need to call this before we'll
 * process any more incoming frames.
 *
 * Parameters: None
 * 
 * Return: Nothing
 */
void IR_BufferingStreamDecoder::readyForNextFrame(void) {
    cli();
    _count = 0;
    _frame_complete = 0;
    _segment_overflows = 0;
    _first_edge = 1;
    sei();
}

/**
 * Tells you when a frame is done. That is, the provided segment buffer is
 * full. Once this function returns true, no new frames will overwrite the
 * segment buffer, giving you time to process it until you call
 * readyForNextFrame().
 *
 * Parameters: None
 *
 * Return: 0 - If there is no frame available
 *         1 - If the frame is completed and ready to process
 */
uint8_t IR_BufferingStreamDecoder::isDone(void) {
    return _frame_complete;
}

/**
 * Tells you how many segments were recorded in the frame. This function only
 * returns a non-zero result after the frame has been completed. That is,
 * until isDone() returns true, this function returns 0.
 *
 * Parameters: None
 *
 * Return: If no complete frame has been received, returns 0.
 *         If a frame has been received, returns the number of segments 
 *         recorded in the segment buffer. NOTE: will never return more than
 *         the number of segments given to setSegmentBuffer().
 */
uint8_t IR_BufferingStreamDecoder::getSegmentCount(void) {
    if (0 == _frame_complete) {
        return 0;
    }

    return _count;
}

/**
 * Tells you how many segments were seen but ignored because the segment
 * buffer was already full.
 *
 * If you're seeing this return > 0, then you have one of two problems:
 *   1. Your segment buffer is not large enough. Increase its size.
 *   2. The IR device you're interfacing with sends frames that start less
 *      than ~32ms after the last edge of the previous. Although this driver
 *      could be modified to support a faster timeout, at this point it's a
 *      fundamental limitation that you cannot easily overcome. In your
 *      processing, try to look for the start of a new frame and discard the
 *      remaining segments. You will miss a frame, but that may be good
 *      enough.
 *
 * This function can be called at any time, but its returned count is most
 * accurate after a frame has been received. When you call 
 * readyForNextFrame(), the count is zeroed out to record the count for 
 * the next frame.
 *
 * Parameters: None
 *
 * Return: The number of segments that were dropped from the current frame.
 *         0xFF indicates that at least 255 were dropped, but possibly more
 *         since we saturate the counter.
 *         0 indicates that either the buffer was large enough, or
 *         readyForNextFrame() was just called.
 */
uint8_t IR_BufferingStreamDecoder::getSegmentOverflowCount(void) {
    return _segment_overflows;
}

/**
 * ISR - Timer1 Capture Interrupt. This function will go into the vector 
 * table. See IR_HwInterface::captureInterrupt() for more details.
 */
ISR(TIMER1_CAPT_vect) {
    IR_InputCaptureInterface.captureInterrupt();
}

/**
 * ISR - Timer1 Overflow Interrupt. This function will go into the vector 
 * table. See IR_HwInterface::overflowInterrupt() for more details.
 */
ISR(TIMER1_OVF_vect) {
    IR_InputCaptureInterface.overflowInterrupt();
}

