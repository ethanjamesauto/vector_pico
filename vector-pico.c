#include "pico/multicore.h"
#include "pico/stdlib.h"
#include <stdio.h>

#include "advmame.h"
#include "drawing.h"

void draw_loop()
{
    while (1) {
        draw_frame();
    }
}

int main()
{
    stdio_init_all();

    const int pos = 2047;
    const int neg = 2048;

    init();
    begin_frame();
    draw_moveto(-neg, -neg);
    draw_to_xyrgb(pos, -neg, 255, 255, 255);
    draw_to_xyrgb(pos, pos, 255, 255, 255);
    draw_to_xyrgb(-neg, pos, 255, 255, 255);
    draw_to_xyrgb(-neg, -neg, 255, 255, 255);
    draw_string("Hello, world!", -1800, 200, 20, 255);
    draw_string("0123456789+=-", -1800, -200, 20, 255);
    end_frame();

    read_data(true);
    multicore_launch_core1(draw_loop);
    while (1) {
        read_data(false);
    }
    return 0;
}
