#include "drawing.h"
#include "hershey_font.h"
#include "asteroids_font.h"

#define CONFIG_FONT_HERSHEY

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