#include "drawing.h"
#include "asteroids_font.h"
#include "build/spi_half_duplex.pio.h"
#include "hershey_font.h"
#include "max5716.h"
#include "util.h"

#define MAX_PTS 4096

#define PINCUSHION_FACTOR 23 // higher number -> less correction

// #define CONFIG_FONT_HERSHEY

#define PIO pio0
#define SM_X 0
#define SM_Y 1

typedef struct {
    int16_t x; // x destination
    int16_t y; // y destination
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

bool frame_ready = 0;

int xpos = 0, ypos = 0; // current position
int rpos = 0, gpos = 0, bpos = 0; // current color

void init()
{
    // set up a square pattern to draw initially
    frame_count[POINT_READ] = 4;
    frame[POINT_READ][0] = (point_t) { -2048, -2048, 0, 0, 0 };
    frame[POINT_READ][1] = (point_t) { 2047, -2048, 0, 0, 0 };
    frame[POINT_READ][2] = (point_t) { 2047, 2047, 0, 0, 0 };
    frame[POINT_READ][3] = (point_t) { -2048, 2047, 0, 0, 0 };

    // set up SPI state machines
    for (int i = 5; i <= 10; i++)
        gpio_set_function(i, GPIO_FUNC_PIO0);
    uint offset = pio_add_program(PIO, &spi_cpha0_cs_program);
    pio_spi_cs_init(PIO, SM_X, offset, 24, 133e6 / (30e6 * 2), 5, 7);
    pio_spi_cs_init(PIO, SM_Y, offset, 24, 133e6 / (30e6 * 2), 8, 10);
}

void begin_frame()
{
    frame_count[POINT_WRITE] = 0;
}

void end_frame()
{
    frame_ready = 1;
}

void draw_string(const char* s, int x, int y, int size, int brightness)
{
    while (*s) {
        char c = *s++;
        x += draw_character(c, x, y, size, brightness);
    }
}

int draw_character(char c, int x, int y, int size, int brightness)
{
#ifdef CONFIG_FONT_HERSHEY
    const hershey_char_t* const f = &hershey_simplex[c - ' '];
    int next_moveto = 1;

    for (int i = 0; i < f->count; i++) {
        int dx = f->points[2 * i + 0];
        int dy = f->points[2 * i + 1];
        if (dx == -1) {
            next_moveto = 1;
            continue;
        }

        dx = (dx * size) * 3 / 4;
        dy = (dy * size) * 3 / 4;

        if (next_moveto)
            draw_moveto(x + dx, y + dy);
        else
            draw_to_xyrgb(x + dx, y + dy, brightness, brightness, brightness);

        next_moveto = 0;
    }

    return (f->width * size) * 3 / 4;
#else
    // Asteroids font only has upper case
    if ('a' <= c && c <= 'z')
        c -= 'a' - 'A';

    const uint8_t* const pts = asteroids_font[c - ' '].points;
    int next_moveto = 1;

    for (int i = 0; i < 8; i++) {
        uint8_t delta = pts[i];
        if (delta == FONT_LAST)
            break;
        if (delta == FONT_UP) {
            next_moveto = 1;
            continue;
        }

        unsigned dx = ((delta >> 4) & 0xF) * size;
        unsigned dy = ((delta >> 0) & 0xF) * size;

        if (next_moveto)
            draw_moveto(x + dx, y + dy);
        else
            draw_to_xyrgb(x + dx, y + dy, brightness, brightness, brightness);

        next_moveto = 0;
    }

    return 12 * size;
#endif
}

void draw_moveto(int x, int y)
{
    brightness(0, 0, 0);
    _draw_lineto(x, y);
}

void draw_to_xyrgb(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    brightness(r, g, b);
    _draw_lineto(x, y);
}

void brightness(uint8_t r, uint8_t g, uint8_t b)
{
    frame[POINT_WRITE][frame_count[POINT_WRITE]].r = r;
    frame[POINT_WRITE][frame_count[POINT_WRITE]].g = g;
    frame[POINT_WRITE][frame_count[POINT_WRITE]].b = b;
}

void _draw_lineto(int x, int y)
{
    x = clamp(x, -2048, 2047);
    y = clamp(y, -2048, 2047);
    frame[POINT_WRITE][frame_count[POINT_WRITE]].x = x;
    frame[POINT_WRITE][frame_count[POINT_WRITE]].y = y;
    frame_count[POINT_WRITE]++;
}

typedef enum {
    X_POSITIVE,
    X_NEGATIVE,
    Y_POSITIVE,
    Y_NEGATIVE
} line_mode;

/**
 * Everything below this line should run as fast as possible and on a separate core
 */
static inline void __always_inline(draw_line)(int16_t x1, int16_t y1, line_mode mode)
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
        int xcorr = x - (xtemp * ytemp * ytemp >> PINCUSHION_FACTOR);
        int ycorr = y - (ytemp * xtemp * xtemp >> PINCUSHION_FACTOR);
        pio_sm_put_blocking(PIO, SM_X, max5716_data_word((xcorr + 2048) << 4));
        pio_sm_put_blocking(PIO, SM_Y, max5716_data_word((ycorr + 2048) << 4));
    }
}

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

void __not_in_flash_func(draw_frame)()
{
    for (int i = 0; i < frame_count[POINT_READ]; i++) {
        point_t p = frame[POINT_READ][i];
        uint16_t x = p.x;
        uint16_t y = p.y;
        uint16_t r = p.r;
        uint16_t g = p.g;
        uint16_t b = p.b;
        if (r == 0 && g == 0 && b == 0)
            jump(x, y);
        else
            draw_line(x, y, X_POSITIVE);
    }
    if (frame_ready) {
        which_frame = !which_frame;
        frame_ready = 0;
    }
}