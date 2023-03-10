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

#include "advmame.h"
#include "settings.h"
#include "spi_dma.h"

#include "hardware/divider.h"
#include <math.h>

uint16_t line_speed = 3;
uint16_t jump_speed = 150;

uint32_t point_buffer[2][MAX_PTS];
uint16_t point_count[2];
int point_pos = 0;
bool which_point = 0;

#define POINT_READ which_point
#define POINT_WRITE !which_point

bool frame_ready = false;

void setup1()
{
    pinMode(FRAME_PIN, OUTPUT);
    spi_dma_init();
}

void loop1()
{
}

enum vector_sm_state { START,
    LINE_X_0,
    LINE_Y_0,
    LINE_X_1,
    LINE_Y_1,
    NEXT_POINT,
    JUMP_X,
    JUMP_Y
};

inline void vector_sm_execute() __attribute__((always_inline));
inline void vector_sm_execute()
{
    static int32_t x = 0;
    static int32_t y = 0;
    static vector_sm_state state = START;
    static uint32_t point = 0;
    static uint8_t brightness;

    // James algorithm variables
    //TODO: consider making some of this stuff non-static and having it recomputed every buffer run, so that memory isn't used as much maybe?
    static int32_t x1, y1;
    static int32_t dx, dy;
    static int32_t b, n, step;
    static int32_t jump_counter;

    for (int i = 0; i < BUFFER_SIZE;) {
        switch (state) {
        case START:
        start: //TODO: waittt, why isn't all this stuff precomputed when a frame is recieved??????????
            brightness = (point_buffer[POINT_READ][point] >> 24) & 0x3f;
            x1 = (point_buffer[POINT_READ][point] >> 12) & 0xfff;
            y1 = point_buffer[POINT_READ][point] & 0xfff;
            dx = x1 - x;
            dy = y1 - y;
            if (brightness == 0) {
                jump_counter = max(abs(dx), abs(dy)) / JUMP_SPEED + 1;
                x = x1;
                y = y1;
                goto jump_x;
            }
            if (abs(dx) >= abs(dy)) {
                b = -dy * x1 / dx + y1;
                step = dx > 0 ? DRAW_SPEED : -DRAW_SPEED;
                x -= step - dx % step;
                goto line_x_0;
            } else {
                b = -dx * y1 / dy + x1;
                step = dy > 0 ? DRAW_SPEED : -DRAW_SPEED;
                y -= step - dy % step;
                goto line_y_0;
            }
            break;
        case LINE_X_0:
        line_x_0:
            hw_divider_divmod_s32_start(x * dy, dx);
            x += step;
            DAC_data[which_dma][i++] = DAC_X | x;
            n = hw_divider_s32_quotient_wait() + b;
            if (i == BUFFER_SIZE) {
                state = LINE_X_1;
                return;
            }
        case LINE_X_1:
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
                    state = LINE_X_0;
                    return;
                }
            }
            goto line_x_0;

        case LINE_Y_0:
        line_y_0:
            hw_divider_divmod_s32_start(y * dx, dy);
            y += step;
            DAC_data[which_dma][i++] = DAC_Y | y;
            n = hw_divider_s32_quotient_wait() + b;
            if (i == BUFFER_SIZE) {
                state = LINE_Y_1;
                return;
            }
        case LINE_Y_1:
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
                    state = LINE_Y_0;
                    return;
                }
            }
            goto line_y_0;

        case JUMP_X:
        jump_x:
            if (jump_counter-- <= 0) {
                goto next_point;
            }
            DAC_data[which_dma][i++] = DAC_X | x;
            if (i == BUFFER_SIZE) {
                state = JUMP_Y;
                return;
            }
        case JUMP_Y:
            DAC_data[which_dma][i++] = DAC_Y | y;
            if (i == BUFFER_SIZE) {
                state = JUMP_X;
                return;
            }
            goto jump_x;

        case NEXT_POINT:
        next_point:
            point++;
            if (point >= point_count[POINT_READ]) {
                point = 0;
                if (frame_ready) {
                    which_point = !which_point;
                    frame_ready = false;
                }
            }
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
    dma_hw->ints0 = 1u << pio0_sm0_ctrl_channel;
}

void setup()
{
    init_settings();
    pinMode(SERIAL_LED, OUTPUT);
    point_count[POINT_READ] = 5;
    point_count[POINT_WRITE] = 1;
    point_buffer[POINT_READ][0] = (0 << 24) | (0 << 12) | 0;
    point_buffer[POINT_READ][1] = (1 << 24) | (4095 << 12) | 0;
    point_buffer[POINT_READ][2] = (1 << 24) | (4095 << 12) | 4095;
    point_buffer[POINT_READ][3] = (1 << 24) | (0 << 12) | 4095;
    point_buffer[POINT_READ][4] = (1 << 24) | (0 << 12) | 0;
    Serial.begin();
    read_data(1);
}

void loop()
{
    static bool start = true;
    update_settings();

    // read from Serial
    if (!frame_ready) {
        if (start) {
            start = false;
            point_count[POINT_WRITE] = 0;
        }
        if (Serial.available() > 0) {
            digitalWriteFast(SERIAL_LED, HIGH);
        } else {
            digitalWriteFast(SERIAL_LED, LOW);
            return;
        }
        advmame_data cmd = read_data(0);
        if (cmd.status == 1) {
            if (point_count[POINT_WRITE] > 0) {
                frame_ready = true;
                start = true;
            }
        } else if (cmd.status == 2 && point_count[POINT_WRITE] < MAX_PTS) {
            point_buffer[POINT_WRITE][point_count[POINT_WRITE]++] = cmd.point; // add the point to the buffer, then increment the point count
        }
    } else {
        // char b = Serial.read();
        delayMicroseconds(100);
    }
}