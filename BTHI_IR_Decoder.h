#ifndef BTHI_IR_DECODER_H
#define BTHI_IR_DECODER_H

#include <platform.h>
#include <stdlib.h>

#define K_DEADTIME_DEFAULT_US  15000

typedef unsigned int command_t;

typedef struct {
	bool level;
	uint32_t duration;
} ir_decoder_edge_t;

class BTHI_IR_Decoder {
private:
	// TODO: Make this configurable size later...
	ir_decoder_edge_t _frame_buffer[64];
	ir_decoder_edge_t _latched_frame_buffer[64];
	uint16_t _latched_frame_length;
    
    uint32_t _last_timestamp;
  	
	uint16_t _sample_index;
    
	uint8_t _frame_available;
	
    uint32_t _k_dead_time;

    uint8_t _pin;

public:
	BTHI_IR_Decoder();
	void setup(void);
	void interrupt(void);
	uint8_t isFrameAvailable(void);
	int16_t copyFrame(ir_decoder_edge_t *dest_buffer, uint16_t buffer_size);
};

extern BTHI_IR_Decoder IR_Decoder;
#endif

