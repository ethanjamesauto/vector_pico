#include "drawing.h"
#include "build/spi_half_duplex.pio.h"
#include "max5716.h"

#define MAX_PTS 4096

#define PIO pio0
#define SM_X 0
#define SM_Y 1

typedef struct {
    int16_t x; // x destination
    int16_t y; // y destination
    bool blank; // if this is true, the program shouldn't bother looking at rgb
    uint8_t r;
    uint8_t g;
    uint8_t b;

    // TODO: add drawing variables
} point_t;

point_t frame[2][MAX_PTS]; // two framebuffers - one for writing to, and one for reading from

uint frame_count[2]; // number of points in each framebuffer

bool which_frame = 0; // which framebuffer is read from
#define POINT_READ which_frame
#define POINT_WRITE !which_frame

int xpos = 0, ypos = 0; // current position

void init()
{
    frame_count[POINT_READ] = 4;
    frame[POINT_READ][0] = (point_t) { -2048, -2048, false, 0, 0, 0 };
    frame[POINT_READ][1] = (point_t) { 2047, -2048, false, 0, 0, 0 };
    frame[POINT_READ][2] = (point_t) { 2047, 2047, false, 0, 0, 0 };
    frame[POINT_READ][3] = (point_t) { -2048, 2047, false, 0, 0, 0 };

    uint offset = pio_add_program(PIO, &spi_cpha0_cs_program);
    pio_spi_cs_init(PIO, SM_X, offset, 24, 133e6 / (50e6 * 2), 5, 7);
    pio_spi_cs_init(PIO, SM_Y, offset, 24, 133e6 / (50e6 * 2), 8, 10);
}

typedef enum {
    X_POSITIVE,
    X_NEGATIVE,
    Y_POSITIVE,
    Y_NEGATIVE
} line_mode;

static inline void __not_in_flash_func(draw_line)(int16_t x1, int16_t y1, line_mode mode)
{
    int x = xpos, y = ypos;
    int8_t sx = x1 - x > 0 ? 1 : -1;
    int8_t sy = y1 - y > 0 ? 1 : -1;
    while (1) {
        if (x == x1 && y == y1) {
            xpos = x;
            ypos = y;
            return;
        }
        if (x != x1)
            x += sx;
        if (y != y1)
            y += sy;

        // floating point ops are WAY too slow on the pico!
        // float xcorr = x * (1.0 - .000000013 * y * y);
        // float ycorr = y * (1.0 - .000000005 * x * x);

        int xtemp = x >> 1;
        int ytemp = y >> 1;
        int xcorr = x - (xtemp * ytemp * ytemp >> 24);
        int ycorr = y - (ytemp * xtemp * xtemp >> 24);
        pio_sm_put_blocking(PIO, SM_X, max5716_data_word((xcorr + 2048) << 4));
        pio_sm_put_blocking(PIO, SM_Y, max5716_data_word((ycorr + 2048) << 4));
    }
}

void draw_frame()
{
    for (int i = 0; i < frame_count[POINT_READ]; i++) {
        point_t p = frame[POINT_READ][i];
        uint16_t x = p.x;
        uint16_t y = p.y;

        draw_line(x, y, X_POSITIVE);
    }
}