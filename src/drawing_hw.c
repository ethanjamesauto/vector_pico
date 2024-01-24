#include "build/spi_half_duplex.pio.h"
#include "drawing.h"
#include "hardware/gpio.h"
#include "max5716.h"
#include "pico/time.h"
#include "settings.h"
#include "util.h"
#include <stdlib.h>

#define MAX_PTS 4096

#define BLANK_PIN 3

#define PIO pio0
#define SM_X 0
#define SM_Y 1

typedef struct {
    int16_t x; // x destination
    int16_t y; // y destination
    int16_t dx;
    int16_t dy;
    bool blank;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    line_mode mode; // which line drawing algorithm to use
    int b; // y-intercept value
    uint16_t shift; // drawing speed
} point_t;

// Global variables
point_t frame[2][MAX_PTS]; // two framebuffers - one for writing to, and one for reading from
uint frame_count[2]; // number of points in each framebuffer

bool which_frame = 0; // which framebuffer is read from
bool frame_ready = 0;

int16_t xpos = 0, ypos = 0; // current position
uint8_t rpos = 0, gpos = 0, bpos = 0; // current color
// End global variables

#define POINT_READ which_frame
#define POINT_WRITE !which_frame

void init()
{
    frame_count[POINT_WRITE] = 0;
    frame_count[POINT_READ] = 0;

    gpio_set_function(BLANK_PIN, GPIO_FUNC_SIO);
    gpio_init(BLANK_PIN);
    gpio_set_dir(BLANK_PIN, GPIO_OUT);
    gpio_put(BLANK_PIN, 0);

    // set up SPI state machines
#ifdef SINGLE_MCP4922
    uint offset = pio_add_program(PIO, &spi_cpha0_cs_program);
    for (int i = 4; i <= 6; i++)
        gpio_set_function(i, GPIO_FUNC_PIO0);
    pio_spi_cs_init(PIO, SM_X, offset, 16, 133e6 / (20e6 * 2), 4, 6);
#elif MCP4922
    uint offset = pio_add_program(PIO, &spi_cpha0_cs_program);
    for (int i = 5; i <= 10; i++)
        gpio_set_function(i, GPIO_FUNC_PIO0);
    pio_spi_cs_init(PIO, SM_X, offset, 16, 133e6 / (20e6 * 2), 5, 7);
    pio_spi_cs_init(PIO, SM_Y, offset, 16, 133e6 / (20e6 * 2), 6, 7);
#else
    uint offset = pio_add_program(PIO, &spi_cpha0_cs_program);
    for (int i = 5; i <= 10; i++)
        gpio_set_function(i, GPIO_FUNC_PIO0);
    pio_spi_cs_init(PIO, SM_X, offset, 24, 133e6 / (24e6 * 2), 5, 7);
    pio_spi_cs_init(PIO, SM_Y, offset, 24, 133e6 / (24e6 * 2), 8, 10);
#endif
}

void begin_frame()
{
    frame_count[POINT_WRITE] = 0;
}

void end_frame()
{
    frame_ready = 1;

    // A new frame cannot be buffered untl the one ready to be drawn (not the one actively being drawn) is drawn.
    // Right now, this method blocks until the frame is drawn using the while loop. There's a delay to stop the
    // loop from hogging the CPU and RAM.
    // TODO: Somehow have core 1 tell core 0 when the frame is done being drawn.
    while (frame_ready) {
        sleep_us(100);
    }
}

void brightness(uint8_t r, uint8_t g, uint8_t b)
{
    if (r == 0 && g == 0 && b == 0) {
        frame[POINT_WRITE][frame_count[POINT_WRITE]].shift = OFF_SHIFT;
        frame[POINT_WRITE][frame_count[POINT_WRITE]].blank = true;
    } else {
        frame[POINT_WRITE][frame_count[POINT_WRITE]].shift = NORMAL_SHIFT;
        frame[POINT_WRITE][frame_count[POINT_WRITE]].blank = false;
    }

    frame[POINT_WRITE][frame_count[POINT_WRITE]].red = r;
    frame[POINT_WRITE][frame_count[POINT_WRITE]].green = g;
    frame[POINT_WRITE][frame_count[POINT_WRITE]].blue = b;
}

void _draw_lineto(int x1, int y1)
{
    if (frame_count[POINT_WRITE] >= MAX_PTS)
        return;
    static int x = 0;
    static int y = 0;

    x1 = clamp(x1, -2048, 2047);
    y1 = clamp(y1, -2048, 2047);

    if (FLIP_X)
        x1 = ~x1; // bitwise NOT turns -2048 into 2047, etc.
    if (FLIP_Y) 
        y1 = ~y1;
    if (SWAP_XY) {
        int temp = x1;
        x1 = y1;
        y1 = temp;
    }

    x1 = map(x1, -2048, 2047, -2010, 2047); // TODO: the dacs have some problems with lower values for some reason
    y1 = map(y1, -2048, 2047, -2010, 2047);

    point_t* p = &frame[POINT_WRITE][frame_count[POINT_WRITE]];

    p->dx = x1 - x;
    p->dy = y1 - y;

    if (abs(p->dx) >= abs(p->dy)) {
        p->b = -p->dy * x1 / p->dx + y1;
        if (p->dx < 0) {
            p->mode = X_NEGATIVE;
        } else {
            p->mode = X_POSITIVE;
        }
    } else {
        p->b = -p->dx * y1 / p->dy + x1;
        if (p->dy < 0) {
            p->mode = Y_NEGATIVE;
        } else {
            p->mode = Y_POSITIVE;
        }
    }

    x = x1;
    y = y1;
    p->x = x1;
    p->y = y1;
    frame_count[POINT_WRITE]++;
}

/**
 * Everything below this line should run as fast as possible and on a separate core
 */
static inline void __always_inline(goto_xy)(int16_t x, int16_t y, bool always_update)
{
    static int old_x;
    static int old_y;

    int xtemp = x >> 1;
    int ytemp = y >> 1;
    int xcorr = x - (xtemp * ytemp * ytemp >> PINCUSHION_FACTOR);
    int ycorr = y - (ytemp * xtemp * xtemp >> PINCUSHION_FACTOR);
        
#ifdef SINGLE_MCP4922
    if (always_update || xcorr != old_x) {
        pio_sm_put_blocking(PIO, SM_X, (DAC_X | (xcorr + 2048)) << 16);
        old_x = xcorr;
    }
    if (always_update || ycorr != old_y) {
        pio_sm_put_blocking(PIO, SM_X, (DAC_Y | (ycorr + 2048)) << 16);
        old_y = ycorr;
    }
#elif MCP4922
    pio_sm_put_blocking(PIO, SM_X, DAC_X | (xcorr + 2048));
    pio_sm_put_blocking(PIO, SM_Y, DAC_Y | (ycorr + 2048));
#else
    pio_sm_put_blocking(PIO, SM_X, max5716_data_word((xcorr + 2048) << 4));
    pio_sm_put_blocking(PIO, SM_Y, max5716_data_word((ycorr + 2048) << 4));
#endif
}

static inline void __always_inline(dwell)(int count)
{
    // can work better or faster without this on some monitors
    for (int i = 0; i < count; i++) { // The original VSTCM used delayNanoseconds(500);
        // sleep_us(1);
        goto_xy(xpos, ypos, true); // writing to the DACs seems to have a more stable output than sleeping
    }
}

static inline void __always_inline(draw_line)(int16_t x1, int16_t y1, int16_t dx, int16_t dy, line_mode mode, int b, uint16_t shift)
{
    int x = xpos, y = ypos;

    switch (mode) {
    case X_POSITIVE:
        while (1) {
            x += shift;
            if (x >= x1)
                goto end;
            y = x * dy / dx + b;
            goto_xy(x, y, false);
        }
    case X_NEGATIVE:
        while (1) {
            x -= shift;
            if (x <= x1)
                goto end;
            y = x * dy / dx + b;
            goto_xy(x, y, false);
        }
    case Y_POSITIVE:
        while (1) {
            y += shift;
            if (y >= y1)
                goto end;
            x = y * dx / dy + b;
            goto_xy(x, y, false);
        }
    case Y_NEGATIVE:
        while (1) {
            y -= shift;
            if (y <= y1)
                goto end;
            x = y * dx / dy + b;
            goto_xy(x, y, false);
        }
    }
end:
    goto_xy(x1, y1, false);
    xpos = x1;
    ypos = y1;
    return;
}

// TODO: delete this (some old test code specifically for oscilloscopes)
static inline void __always_inline(jump)(int16_t x, int16_t y)
{
    xpos = x;
    ypos = y;

    int xtemp = x >> 1;
    int ytemp = y >> 1;
    int xcorr = x - (xtemp * ytemp * ytemp >> PINCUSHION_FACTOR);
    int ycorr = y - (ytemp * xtemp * xtemp >> PINCUSHION_FACTOR);
    for (int i = 0; i < 50; i++) {
        pio_sm_put_blocking(PIO, SM_X, max5716_data_word((xcorr + 2048) << 4));
        pio_sm_put_blocking(PIO, SM_Y, max5716_data_word((ycorr + 2048) << 4));
    }
}

static inline void __always_inline(update_rgb)(uint8_t r, uint8_t g, uint8_t b)
{
    if (r != rpos || g != gpos || b != bpos) {
        rpos = r;
        gpos = g;
        bpos = b;
        // dwell(OFF_DWELL0); TODO: maybe turn this on
    }
}

static inline bool __always_inline(blank)(bool blank)
{
    static int beam_on = 0;
    if (blank && beam_on) {
        gpio_put(BLANK_PIN, 0);
        beam_on = 0;
        return 1;
    } else if (!blank && !beam_on) {
        gpio_put(BLANK_PIN, 1);
        beam_on = 1;
        return 1;
    }
    return 0;
}

void draw_frame()
{
    for (int i = 0; i < frame_count[POINT_READ]; i++) {
        point_t p = frame[POINT_READ][i];
        // if (p.red == 0 && p.green == 0 && p.blue == 0)
        //     jump(p.x, p.y);
        // else {
        // update_rgb(p.red, p.green, p.blue);
        bool updated = blank(p.blank);
        if (updated)
            dwell(OFF_DWELL0);
        else
            dwell(OFF_DWELL1);
        draw_line(p.x, p.y, p.dx, p.dy, p.mode, p.b, p.shift);
        dwell(OFF_DWELL2);
        //}
    }
    if (frame_ready) {
        which_frame = !which_frame;
        frame_ready = 0;
    }
}