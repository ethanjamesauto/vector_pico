#include "drawing.h"
#include "pico/stdlib.h"
#include <stdio.h>

void vector_mame()
{
    static enum {
        SYNCING,
        READING
    } state
        = SYNCING;

    static uint cmd;
    static int pos = 0;

    int c = getchar();

    switch (state) {

    // go to READING state from SYNCING when 8 0s are received
    case SYNCING:
        if (c == 0) {
            if (++pos == 8) {
                pos = 0;
                state = READING;
                begin_frame();
            }
        } else {
            pos = 0;
        }
        break;

    case READING:
        cmd = (cmd << 8) | c;
        if (++pos == 4) {
            pos = 0;
            if (cmd == 0x01010101) {
                state = SYNCING;
                end_frame();
            } else {
                int x = ((cmd >> 12) & 0xfff) - 2048;
                int y = (cmd & 0xfff) - 2048;
                int brightness = (cmd >> 24) & 0x3f;
                brightness <<= 2;
                draw_to_xyrgb(x, y, brightness, brightness, brightness);
            }
        }
        break;
    }
    return;
}