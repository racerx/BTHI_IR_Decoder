/*----------------------------------------------------------------------------------
 * Streaming Decode Example using the BTHI Universal IR decoding library.
 *
 * This example uses a streaming decoding strategy for the Samsung protocol.
 */
#include <BTHI_IR_Decoder.h>

/**
 * This is an example of how you can build a streaming decoder that uses MUCH 
 * less RAM than using the IR_BufferingStreamDecoder. It doesn't come for free;
 * you'll need to write a bit more code and it will be a lot harder to debug.
 *
 * This implementation uses a state machine to track where we are in the frame
 * and uses otherwise similar approach to the IR_BufferingStreamDecoder when
 * it comes to making the frame available to the application.
 */
class SamsungStreamingDecoder : 
public IR_StreamDecoder {
private:
  enum decode_state_tag {
    WAITING_FOR_FIRST_EDGE,
    WAITING_FOR_SOF_1,
    WAITING_FOR_SOF_2,
    WAITING_FOR_BIT_TOP,
    WAITING_FOR_BIT_BOTTOM,
    WAITING_FOR_FRAME_TO_END
  };

  enum decode_state_tag _state;
  uint32_t _receive_data;
  uint8_t _bits_decoded;
  uint8_t _malformed_frame_count;
  uint8_t _frame_available;

  /**
   * Increments the count of frames that are malformed. Saturates at 255.
   */
  void recordFrameError(void) {
    if (_malformed_frame_count < 0xFF) {
      _malformed_frame_count++;
    }
  }

  /**
   *Internal method for resetting state to receive another frame.
   */
  void resetState(void) {
    _state = WAITING_FOR_FIRST_EDGE;
    _bits_decoded = 0;
    _frame_available = 0;
  }

public:
  SamsungStreamingDecoder(void) {
    _malformed_frame_count = 0;
    resetState();
  }

  /**
   * This will get called for each edge on the incoming waveform.
   */
  void edgeEvent(uint16_t duration) {
    if (0 != _frame_available) {
      /* Ignore the edge */
      return;
    }

    switch (_state) {
    case WAITING_FOR_FIRST_EDGE:
      /* This is our first edge, ignore it and wait for the first
       * full segment,
       */
      _state = WAITING_FOR_SOF_1;
      _receive_data = 0;
      break;

    case WAITING_FOR_SOF_1:
      /* Looking for the first 4.5ms segment */
      if (IR_DURATION_MATCH_US(duration, 4500, 200)) {
        _state = WAITING_FOR_SOF_2;
      } 
      else {
        recordFrameError();
        _state = WAITING_FOR_FIRST_EDGE;
      }
      break;

    case WAITING_FOR_SOF_2:
      /* Looking for the second 4.5ms segment */
      if (IR_DURATION_MATCH_US(duration, 4500, 200)) {
        _state = WAITING_FOR_BIT_TOP;
      } 
      else {
        recordFrameError();
        _state = WAITING_FOR_FIRST_EDGE;
      }
      break;

    case WAITING_FOR_BIT_TOP:
      /* The top half of a bit is always about 560us */
      if (IR_DURATION_MATCH_US(duration, 560, 100)) {
        _state = WAITING_FOR_BIT_BOTTOM;
      } 
      else {
        recordFrameError();
        _state = WAITING_FOR_FIRST_EDGE;
      }
      break;

    case WAITING_FOR_BIT_BOTTOM:
      /* The bottom half of a bit determines whether it's a 1 or a
       				 * 0.  If it's about 560us, then it's a 0. Otherwise a 1. */
      if (IR_DURATION_MATCH_US(duration, 560, 100)) {
        _receive_data <<= 1;
      } 
      else {
        _receive_data <<= 1;
        _receive_data |= 1;
      }

      _bits_decoded++;

      if (32 == _bits_decoded) {
        /* Time to stop decoding */
        _state = WAITING_FOR_FRAME_TO_END;
      } 
      else {
        /* Go back to waiting for the next bit */
        _state = WAITING_FOR_BIT_TOP;
      }
      break;

    case WAITING_FOR_FRAME_TO_END:
      /* Ignore the segment */
      break;
    }
  }

  /**
   * This is called after the hardware layer thinks there is no more frame.
   */
  void endOfFrameEvent(void) {
    uint8_t frame_ok;

    /* Reset the state machine and record that the frame is complete */
    if (_state == WAITING_FOR_FRAME_TO_END) {
      frame_ok = 1;
    } 
    else {
      frame_ok = 0;
    }

    resetState();
    _frame_available = frame_ok;
  }

  uint8_t isFrameAvailable(void) {
    return _frame_available;
  }

  void readyForNextFrame(void) {
    cli();
    resetState();
    sei();
  }

  uint32_t getReceiveData(void) {
    return _receive_data;
  }
};

SamsungStreamingDecoder decoder;

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- BTHI Streaming Decode Example for Samsung Protocol ---\n");

  // Use Pin 8 (the input capture pin on the UNO)
  IR_InputCaptureInterface.setup(&decoder, 8, IR_POLARITY_AUTO);
}

void loop() {
  uint32_t data;
  
  if (decoder.isFrameAvailable()) {
    data = decoder.getReceiveData();
    Serial.print("Received: ");
    Serial.print(codeToString(data));
    Serial.print(" (0x");
    Serial.print(data, HEX);
    Serial.println(")");

    // This will allow the decoder to accept another frame 
    // (overwriting the one we just printed out)
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
    case 0xE0E0E01F:
      return "VOL  (^)";
      
    case 0xE0E0D02F:
      return "VOL  (v)";
      
    case 0xE0E048B7:
      return "CHAN (^)";
      
    case 0xE0E008F7:    
      return "CHAN (v)";
  }
  
  return "UNKNOWN";
}

