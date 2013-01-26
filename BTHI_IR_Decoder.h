#ifndef BTHI_IR_DECODER_H
#define BTHI_IR_DECODER_H

#include <platform.h>
#include <stdlib.h>

typedef struct {
	uint16_t duration;
} ir_segment_t;

typedef enum {
	IR_POLARITY_LOW = 0,
	IR_POLARITY_HIGH,
	IR_POLARITY_AUTO
} ir_polarity_t;

class IR_StreamDecoder {
public:
	virtual void endDecode(void);
	virtual void decodeEdge(uint16_t duration);
};

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
	void decodeEdge(uint16_t duration);
	void endDecode(void);

	void setSegmentBuffer(ir_segment_t *segments, 
		uint8_t num_segments);
	void debugPrintFrame(void);
	void receiveNextFrame(void);
	uint8_t isDone(void);
	uint8_t getSegmentCount(void);
	uint8_t getSegmentOverflowCount(void);
	uint8_t clearSegmentOverflowCount(void);
};

extern IR_HwInterface IR_InputCaptureInterface;

#endif

