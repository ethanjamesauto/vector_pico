/**
 * Ethan James (ethanjamesauto)
 * This is a rough port of osresearch/vst for the Raspberry Pi Pico. It's still a work in progress and might not work correctly.
 */

// Original header:
/**
 *  V. Hunter Adams (vha3)
                Code based on examples from Raspberry Pi Co
                Sets up two DMA channels. One sends samples at audio rate to
 the DAC, (data_chan), and the other writes to the data_chan DMA control
 registers (ctrl_chan). The control channel writes to the data channel, sending
 one period of a sine wave thru the DAC. The control channel is chained to the
 data channel, so it is re-triggered after the data channel is finished. The
 data channel is chained to the control channel, so it is restarted as soon as
 the control channel finishes. The data channel is paced by a timer to perform
 transactions at audio rate.
 */
// TODO: include C:\\Users\\Ethan\\AppData\\Local\\Arduino15\\packages\\rp2040\\hardware\\rp2040\\2.7.1

#include "spi_dma.h"
#include <math.h>

#define SERIAL_LED 25
#define FRAME_PIN 8
#define MAX_PTS 4096

uint16_t line_speed = 3;
uint16_t jump_speed = 150;

uint32_t point_buffer[2][MAX_PTS];
uint16_t point_count[2];
int point_pos = 0;
bool which_point = 0;

#define POINT_READ which_point
#define POINT_WRITE !which_point

bool frame_ready = 0;

void setup()
{
    pinMode(FRAME_PIN, OUTPUT);
    spi_dma_init();
}

void loop()
{
}

enum vector_sm_state { START,
    LINE_10,
    LINE_11,
    LINE_X,
    LINE_Y,
    LINE_DONE,
    JUMP
};

#define JUMP_SPEED 8
#define DRAW_SPEED 3

inline void vector_sm_execute(int i) __attribute__((always_inline));
inline void vector_sm_execute(int i)
{
    static uint16_t x = 0;
    static uint16_t y = 0;
    static vector_sm_state state = START;
    static uint16_t point = 0;
    static uint16_t jump_ctr;
    static uint8_t brightness;

    // breesenham variables
    static int32_t dx, dy, sx, sy, err, e2; // TODO: maybe change the types
    static uint16_t x1, y1;
    for (int k = 0; k < 1000; k++) {
        switch (state) {
        case START:
            if (point_count[POINT_READ] == 0) {
                state = LINE_DONE;
                break;
            }
            brightness = (point_buffer[POINT_READ][point] >> 24) & 0x3f;
            x1 = ((point_buffer[POINT_READ][point] >> 12) & 0xfff);
            y1 = (point_buffer[POINT_READ][point] & 0xfff);
            if (brightness == 0 || point == 0) {
                jump_ctr = ((abs(x1 - x) + abs(y1 - y)) >> JUMP_SPEED) + 2;
                state = JUMP;
                break;
            }
            x1 >>= DRAW_SPEED;
            y1 >>= DRAW_SPEED;
            x >>= DRAW_SPEED;
            y >>= DRAW_SPEED;
            dx = abs(x1 - x);
            dy = abs(y1 - y);
            sx = x < x1 ? 1 : -1;
            sy = y < y1 ? 1 : -1;
            err = (dx > dy ? dx : -dy) / 2;
            state = LINE_10;
            break;
        case JUMP:
            if (jump_ctr <= 0) {
                x = x1;
                y = y1;
                point++;
                if (point >= point_count[POINT_READ]) {
                    point = 0;
                }
                state = START;
                break;
            }
            jump_ctr--;
            if (jump_ctr % 2 == 0) {
                DAC_data[which_dma][i] = DAC_X | x1;
            } else {
                DAC_data[which_dma][i] = DAC_Y | y1;
            }
            return;
        case LINE_X:
            break;
        case LINE_Y:
            break;
        case LINE_10:
            if (x == x1 && y == y1) {
                state = LINE_DONE;
                break;
            }
            e2 = err;
            if (e2 > -dx) {
                err -= dy;
                x += sx;
                DAC_data[which_dma][i] = DAC_X | (x << DRAW_SPEED);
                state = LINE_11;
                return;
            }
            state = LINE_11;
            break;
        case LINE_11:
            if (e2 < dy) {
                err += dx;
                y += sy;
                DAC_data[which_dma][i] = DAC_Y | (y << DRAW_SPEED);
                state = LINE_10;
                return;
            }
            state = LINE_10;
            break;
        case LINE_DONE:
            x = (point_buffer[POINT_READ][point] >> 12) & 0xfff;
            y = point_buffer[POINT_READ][point] & 0xfff;
            point++;
            if (point >= point_count[POINT_READ]) {
                point = 0;
            }
            state = START;
            break;
        }
    }
}

void control_complete_isr()
{
    // if (!done)
    //     Serial.println("Can't keep up, only reached point " + String(buffer_pos));
    which_dma = !which_dma;
    // buffer_pos = 0;
    done = false;
    address_pointer = &DAC_data[which_dma][0];
    static bool dir = 1;
    // dir = !dir;
    static int n = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        vector_sm_execute(i);
    }
    dma_hw->ints0 = 1u << CTRL_CHANNEL;
}

void setup1()
{
    Serial.begin();
    pinMode(SERIAL_LED, OUTPUT);
    point_count[POINT_READ] = 5;
    point_count[POINT_WRITE] = 1;
    point_buffer[POINT_READ][0] = (0 << 24) | (0 << 12) | 0;
    point_buffer[POINT_READ][1] = (1 << 24) | (4095 << 12) | 0;
    point_buffer[POINT_READ][2] = (1 << 24) | (4095 << 12) | 4095;
    point_buffer[POINT_READ][3] = (1 << 24) | (0 << 12) | 4095;
    point_buffer[POINT_READ][4] = (1 << 24) | (0 << 12) | 0;
}

void loop1()
{
    static uint32_t cmd = 0;
    static char cmd_count = 0;
    static bool waiting = true;

    // read from Serial
    // if (!frame_ready) {
    if (Serial.available() > 0) {
        digitalWriteFast(SERIAL_LED, HIGH);
        char b = Serial.read();
        if (waiting && b != 0) { // if we receive a nonzero byte, we're ready to start the next frame
            waiting = false;
            // frame_ready = 0;
            point_count[POINT_WRITE] = 0;
        }
        if (!waiting) {
            cmd = (cmd << 8) | b; // build up a 32-bit command
            if (++cmd_count == 4) { // see if we have a full command
                cmd_count = 0; // reset the command counter
                if (cmd == 0x01010101) { // if we receive a "done" command
                    // frame_ready = true;
                    if (point_count[POINT_WRITE] > 0)
                        which_point = !which_point;
                    waiting = true; // go back to waiting
                } else if (point_count[POINT_WRITE] < MAX_PTS) {
                    point_buffer[POINT_WRITE][point_count[POINT_WRITE]++] = cmd; // add the point to the buffer, then increment the point count
                }
                if (cmd == 0) {
                    // Somehow we reached the gap between one frame and another without recieving a done command.
                    // This can occur if the pico starts recieving data mid-frame and is unable to sync. If this happens, just go back to waiting for the next frame.
                    waiting = true;
                }
            }
        }
        digitalWriteFast(SERIAL_LED, LOW);
    }
    /*} else {
        char b = Serial.read();
        //delayMicroseconds(100);
    }*/
}