/*----------------------------------------------------------------------------------
 * Example using the Universal IR decoding library with a buffering decoder that
 * understands the Apple IR protocol (that remote that comes with AppleTV or your 
 * laptop/iMac).
 *
 * I just quickly experimented with it, I didn't figure out what's going on with 
 * that second command that's sent after the middle button or the play/pause 
 * button.
 */
#include <BTHI_IR_Decoder.h>

IR_BufferingStreamDecoder decoder;

/* Room for 80 segments. Frames are 67 edges long. */
#define NUM_SEGMENTS  80

ir_segment_t g_segment_buffer[NUM_SEGMENTS];

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- BTHI Apple Decoding Example ---\n");

  /* Set up the decoder first. Give it a buffer to store the edges in */
  decoder.setSegmentBuffer(g_segment_buffer, NUM_SEGMENTS);
    
  /* Use Pin 8 (the input capture pin on the UNO) */
  IR_InputCaptureInterface.setup(&decoder, 8, IR_POLARITY_AUTO);
}

void loop() {
  uint32_t data;
  int8_t res;
    
  if (decoder.isFrameAvailable()) {
    res = decodeFrameApple(&decoder, &data);
    if (res == IR_E_OK) {
      Serial.print("Received: ");
      Serial.print(codeToString(data));
      Serial.print(" (0x");
      Serial.print(data, HEX);
      Serial.println(")");
    } else if (res == IR_E_INVALID_START_OF_FRAME) {
      Serial.println("ERROR: Invalid start of frame!");
    } else if (res == IR_E_SHORT_FRAME) {
      Serial.println("ERROR: Short frame!");
    } else {
      Serial.println("ERROR: Unknown!");
    }
      
    /* This will allow the decoder to accept another frame 
     * (overwriting the one we just printed out)
     */
    decoder.readyForNextFrame();
  }
}

/**
 * Converts from the numeric code to an ASCII string. 
 * I pulled these from my Samsung TV. Your mileage may vary.
 * You also may be able to ignore the top 2 bytes and get a more efficient
 * version of this function.
 */
char *codeToString(uint32_t code) {
  switch (code) {
    case 0x77E1508C:
      return "[^]";
      
    case 0x77E1908C:
      return "[<]";
      
    case 0x77E1308C:
      return "[v]";
      
    case 0x77E1608C:
      return "[>]";
      
    case 0x77E13A8C:    
      return "[*]";
      
    case 0x77E1C08C:    
      return "MENU";
      
    case 0x77E1FA8C:    
      return "Play/Pause";
  }
  
  return "UNKNOWN";
}
