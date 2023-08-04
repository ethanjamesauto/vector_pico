#include "drawing.h"
#include "pico/stdlib.h"
#include <stdio.h>

#define CHAR_BUF_SIZE 128

void vector_mame()
{
    static enum {
        SYNCING,
        READING
    } state
        = SYNCING;

    static uint cmd;
    static int pos = 0;

    //getchar() is slow, so I had to write this junk instead
    static int buf_pos = CHAR_BUF_SIZE;
    static char buf[CHAR_BUF_SIZE];

    if (buf_pos == CHAR_BUF_SIZE) {
        buf_pos = 0;
        //fgets(buf, CHAR_BUF_SIZE, stdin);
        fread(buf, CHAR_BUF_SIZE, 1, stdin);
    }

    int c = buf[buf_pos++];

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