#include <BTHI_IR_Decoder.h>

void setup() {
    IR_Decoder.setup();
    Serial.begin(115200);
    Serial.println("Waiting for Frames:");
}

void loop() {
    int16_t res;
    ir_decoder_edge_t frame_buffer[64];

    if (IR_Decoder.isFrameAvailable()) {
        res = IR_Decoder.copyFrame(&frame_buffer[0], sizeof(frame_buffer));
        if (res > 0) {
            // Process the frame
            Serial.println("Frame Received!");
        }
    }
}

