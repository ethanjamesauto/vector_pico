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

uint8_t line_speed = 5;
uint8_t jump_speed = 75;

uint32_t point_buffer[2][MAX_PTS];
int point_count[2];
int point_pos = 0;
bool which_point = 0;

void setup()
{
    pinMode(FRAME_PIN, OUTPUT);
    spi_dma_init();
}

void loop()
{
    bool buf = !which_point;
    // Serial.println("Drawing frame with size " + String(point_count[buf]) + ", point 0 is " + String(point_buffer[buf][0]));
    digitalWrite(FRAME_PIN, HIGH);
    bool pin = true;

    for (int i = 0; i < point_count[buf]; i++) {
        if (pin && i > point_count[buf] / 10) {
            digitalWrite(FRAME_PIN, LOW);
            pin = false;
        }
        uint16_t x1 = (point_buffer[buf][i] >> 12) & 0xfff;
        uint16_t y1 = point_buffer[buf][i] & 0xfff;
        uint8_t brightness = (point_buffer[buf][i] >> 24) & 0b111111;
        if (brightness == 0 || i == 0) {
            uint16_t x2 = (point_buffer[buf][prev(i, point_count[buf])] >> 12) & 0xfff;
            uint16_t y2 = point_buffer[buf][prev(i, point_count[buf])] & 0xfff;
            int dist = sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
            lineto(x2, y2, x1, y1, jump_speed);
            dwell(x1, y1, dist / 10);
        } else {
            uint16_t x2 = (point_buffer[buf][prev(i, point_count[buf])] >> 12) & 0xfff;
            uint16_t y2 = point_buffer[buf][prev(i, point_count[buf])] & 0xfff;
            lineto(x2, y2, x1, y1, line_speed);
        }
    }
}

// TODO: maybe get rid of this, as the first point should always be a jump
inline int prev(int i, int max)
{
    int prev = i - 1;
    if (prev < 0) {
        prev = max - 1;
    }
    return prev;
}

void dwell(uint16_t x, uint16_t y, uint8_t delay)
{
    for (int i = 0; i < delay; i++) {
        if (i % 2 == 0) {
            DAC_data[!which_dma][buffer_pos++] = DAC_A | x;
        } else {
            DAC_data[!which_dma][buffer_pos++] = DAC_B | y;
        }
        if (buffer_pos >= BUFFER_SIZE) {
            done_buffering();
        }
    }
}

// draw a line using breesenham
void lineto(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t step)
{
    x1 /= step;
    y1 /= step;
    x2 /= step;
    y2 /= step;

    uint16_t dx = abs(x2 - x1);
    uint16_t dy = abs(y2 - y1);
    int8_t sx = x1 < x2 ? 1 : -1;
    int8_t sy = y1 < y2 ? 1 : -1;
    int32_t err = (dx > dy ? dx : -dy) / 2, e2; // TODO: make int16_t?

    for (;;) {
        if (x1 == x2 && y1 == y2) {
            break;
        }
        e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x1 += sx;
            DAC_data[!which_dma][buffer_pos++] = DAC_A | (x1 * step);
            if (buffer_pos >= BUFFER_SIZE) {
                done_buffering();
            }
        }

        if (e2 < dy) {
            err += dx;
            y1 += sy;
            DAC_data[!which_dma][buffer_pos++] = DAC_B | (y1 * step);
            if (buffer_pos >= BUFFER_SIZE) {
                done_buffering();
            }
        }
    }
}

void setup1()
{
    Serial.begin();
    pinMode(SERIAL_LED, OUTPUT);
    point_count[1] = 5;
    point_count[0] = 0;
    point_buffer[1][0] = (0 << 24) | (0 << 12) | 0;
    point_buffer[1][1] = (1 << 24) | (4095 << 12) | 0;
    point_buffer[1][2] = (1 << 24) | (4095 << 12) | 4095;
    point_buffer[1][3] = (1 << 24) | (0 << 12) | 4095;
    point_buffer[1][4] = (1 << 24) | (0 << 12) | 0;
}

void loop1()
{
    static uint32_t cmd = 0;
    static char cmd_count = 0;
    static bool waiting = true;

    // read from Serial
    while (Serial.available() > 0) {
        digitalWriteFast(SERIAL_LED, HIGH);
        char b = Serial.read();
        if (waiting && b != 0) { // if we receive a nonzero byte, we're ready to start the next frame
            waiting = false;
            point_count[which_point] = 0;
        }
        if (!waiting) {
            cmd = (cmd << 8) | b; // build up a 32-bit command
            if (++cmd_count == 4) { // see if we have a full command
                cmd_count = 0; // reset the command counter
                if (cmd == 0x01010101) { // if we receive a "done" command
                    waiting = true; // go back to waiting
                    which_point = !which_point; // switch to the other buffer
                } else if (point_count[which_point] < MAX_PTS) {
                    point_buffer[which_point][point_count[which_point]++] = cmd; // add the point to the buffer, then increment the point count
                }
                if (cmd == 0) {
                    /*
                    Somehow we reached the gap between one frame and another without recieving a done command.
                    This can occur if the pico starts recieving data mid-frame and is unable to sync. If this happens, just go back to waiting for the next frame.
                    */
                    waiting = true;
                }
            }
        }
        digitalWriteFast(SERIAL_LED, LOW);
    }
}