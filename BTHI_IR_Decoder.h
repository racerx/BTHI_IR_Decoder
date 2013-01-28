/*---------------------------------------------------------------------------
 * Streaming Infrared Decoder Library
 *
 * Copyright (c) 2013, Bryan Thomas (BTHI) and Christopher Myers
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *---------------------------------------------------------------------------
 * See BTHI_IR_Decoder.cpp for more information.
 */
 
#ifndef BTHI_IR_DECODER_H
#define BTHI_IR_DECODER_H

#include <platform.h>
#include <stdlib.h>

/**
 * This macro can tell you whether the duration in ticks corresponds to a
 * range of microseconds.
 */
#define IR_DURATION_MATCH_US(actual_ticks, expected_us, tolerance_us) \
    ( \
    (actual_ticks >= ((expected_us - tolerance_us) * (1e-6 / 5e-7))) && \
    (actual_ticks <= ((expected_us + tolerance_us) * (1e-6 / 5e-7))) \
    )

/**
 * Return codes that can be used for decode routines.
 */
#define IR_E_INVALID_START_OF_FRAME     -2
#define IR_E_SHORT_FRAME                -1
#define IR_E_OK                         0

/* Structure to hold segment information. We used a structure so that if we
 * ran into a protocol that needed both the duration and level, we'd be able
 * to accomodate it.
 */
typedef struct {
	uint16_t duration;
} ir_segment_t;

/* Enum to define the different polarity options we support. */
typedef enum {
	IR_POLARITY_LOW = 0,
	IR_POLARITY_HIGH,
	IR_POLARITY_AUTO
} ir_polarity_t;

/**
 * Specifies the StreamDecoder interface which serves as the delegate to the
 * IR_HwInterface object. That is, this is the contract between the
 * IR_HwInterface and any decoder.
 */
class IR_StreamDecoder {
public:
	virtual void edgeEvent(uint16_t duration);
	virtual void endOfFrameEvent(void);
};

/**
 * This class represents the interface with the Arduino hardware. It is
 * responsible for extracting waveform data and providing it to a separate
 * IR_StreamDecoder for analysis. It is not concerned with the meaning of the
 * data it receives.
 */
class IR_HwInterface {
private:
	uint8_t _pin;
	IR_StreamDecoder *_decoder;

public:
	IR_HwInterface();
	void setup(IR_StreamDecoder *stream_decoder, uint8_t pin, 
			ir_polarity_t polarity);
	void captureInterrupt();
	void overflowInterrupt();
};

/**
 * Decoder delegate implementation that buffers all of the waveform segments
 * it sees for later analysis instead of decoding them on the fly. This is
 * good for reverse engineering a protocol and serves as a
 * manufacturer-independent example that we can ship.
 */
class IR_BufferingStreamDecoder : public IR_StreamDecoder {
private:
	ir_segment_t *_segments;
	uint8_t _max_segments;
	uint8_t _count;
	uint8_t _segment_overflows;
	uint8_t _frame_complete;
	uint8_t _first_edge;

public:
	IR_BufferingStreamDecoder(void);
	void edgeEvent(uint16_t duration);
	void endOfFrameEvent(void);

	void setSegmentBuffer(ir_segment_t *segments, 
		uint8_t num_segments);
    ir_segment_t *getSegmentBuffer(void);
	void debugPrintFrame(void);
	void readyForNextFrame(void);
	uint8_t isFrameAvailable(void);
	uint8_t getSegmentCount(void);
	uint8_t getSegmentOverflowCount(void);
};

extern int8_t decodeFrameApple(
		IR_BufferingStreamDecoder *bufferedDecoder, 
		uint32_t *data);
extern int8_t decodeFrameSamsung(
		IR_BufferingStreamDecoder *bufferedDecoder, 
		uint32_t *data);

extern IR_HwInterface IR_InputCaptureInterface;

#endif

