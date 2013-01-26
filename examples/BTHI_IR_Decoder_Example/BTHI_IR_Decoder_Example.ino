/*----------------------------------------------------------------------------------
 * Example for the Universal IR decoding library.
 *
 * This example uses a buffered decoding strategy. When a frame is seen, it's 
 * printed out and then the next frame can be received.
 */
#include <BTHI_IR_Decoder.h>

IR_BufferingStreamDecoder decoder;

// Room for 128 segments.  Most remotes that we've tested with have only about 
// 60-70 segments in their commands 
#define NUM_SEGMENTS  128

ir_segment_t g_segment_buffer[NUM_SEGMENTS];

void setup() {
    Serial.begin(115200);
    Serial.println("\n--- BTHI IR Decoding Example ---\n");
    
    // Set up the decoder first. Give it a buffer to store the edges in 
    decoder.setSegmentBuffer(g_segment_buffer, NUM_SEGMENTS);
    // Use Pin 8 (the input capture pin on the UNO)
    IR_InputCaptureInterface.setup(&decoder, 8, IR_POLARITY_AUTO);
}

void loop() {
  
    if (decoder.isDone()) {
      Serial.println("Frame Received!");
      decoder.debugPrintFrame();
      
      // This will allow the decoder to accept another frame 
      // (overwriting the one we just printed out)
      decoder.receiveNextFrame();
    }
}

