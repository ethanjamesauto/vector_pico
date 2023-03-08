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

#include "hardware/divider.h"
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
    LINE_20,
    LINE_11,
    LINE_21,
    NEXT_POINT,
    JUMP,
};

// An example step calculation:
// My monitor has a slew rate of about 100us from min to max - 100us / 4096 = 24.41ns per DAC step.
// The spi frequency is 22.17Mhz / 18 = 1.23Mhz. 1/1.23Mhz = 813ns DAC per step.
// 813ns / 24.41ns = 33.3 DAC steps are needed to run the monitor at full speed.
#define STEP 32

inline void vector_sm_execute() __attribute__((always_inline));
inline void vector_sm_execute()
{
    static int32_t x = 0;
    static int32_t y = 0;
    static vector_sm_state state = START;
    static uint32_t point = 0;
    static uint8_t brightness;

    // James algorithm variables
    static int32_t x1, y1;
    static int32_t dx, dy;
    static int32_t tmp, mult, b, n, step; // TODO: will tmp overflow?

    for (int i = 0; i < BUFFER_SIZE;) {
        switch (state) {
        case START:
        start:
            brightness = (point_buffer[POINT_READ][point] >> 24) & 0x3f;
            x1 = (point_buffer[POINT_READ][point] >> 12) & 0xfff;
            y1 = point_buffer[POINT_READ][point] & 0xfff;
            dx = x1 - x;
            dy = y1 - y;
            if (abs(dx) >= abs(dy)) {
                tmp = x * dy;
                b = -dy * x1 / dx + y1;
                step = dx > 0 ? STEP : -STEP;
                x -= step - dx % step;
                mult = step * dy;
                goto line_10;
            } else {
                tmp = y * dx;
                b = -dx * y1 / dy + x1;
                step = dy > 0 ? STEP : -STEP;
                y -= step - dy % step;
                mult = step * dx;
                goto line_20;
            }
            break;
        case LINE_10:
        line_10:
            tmp += mult;
            hw_divider_divmod_s32_start(tmp, dx);
            x += step;
            DAC_data[which_dma][i++] = DAC_X | x;
            n = hw_divider_s32_quotient_wait() + b;
            if (i == BUFFER_SIZE) {
                state = LINE_11;
                return;
            }
        case LINE_11:
            if (x == x1) {
                y = y1;
                DAC_data[which_dma][i++] = DAC_Y | y;
                if (i == BUFFER_SIZE) {
                    state = NEXT_POINT;
                    return;
                }
                goto next_point;
            }
            if (n != y) {
                y = n;
                DAC_data[which_dma][i++] = DAC_Y | y;
                if (i == BUFFER_SIZE) {
                    state = LINE_10;
                    return;
                }
            }
            goto line_10;
        case LINE_20:
        line_20:
            tmp += mult;
            hw_divider_divmod_s32_start(tmp, dy);
            y += step;
            DAC_data[which_dma][i++] = DAC_Y | y;
            n = hw_divider_s32_quotient_wait() + b;
            if (i == BUFFER_SIZE) {
                state = LINE_21;
                return;
            }
        case LINE_21:
            if (y == y1) {
                x = x1;
                DAC_data[which_dma][i++] = DAC_X | x;
                if (i == BUFFER_SIZE) {
                    state = NEXT_POINT;
                    return;
                }
                goto next_point;
            }
            if (n != x) {
                x = n;
                DAC_data[which_dma][i++] = DAC_X | x;
                if (i == BUFFER_SIZE) {
                    state = LINE_20;
                    return;
                }
            }
            goto line_20;
        case NEXT_POINT:
        next_point:
            point++;
            if (point >= point_count[POINT_READ])
                point = 0;
            goto start;
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
    vector_sm_execute();
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