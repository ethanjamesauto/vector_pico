#include "pico/multicore.h"
#include "pico/stdlib.h"
#include <stdio.h>

#include "advmame.h"
#include "drawing.h"
#include "test_pattern.h"

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
    //*
    draw_moveto(-neg, -neg);
    draw_to_xyrgb(pos, -neg, 255, 255, 255);
    draw_to_xyrgb(pos, pos, 255, 255, 255);
    draw_to_xyrgb(-neg, pos, 255, 255, 255);
    draw_to_xyrgb(-neg, -neg, 255, 255, 255);
    draw_string("VSTCM-Pico", -1800, -100, 20, 255);
    draw_string("2023 Ethan James", -1800, -500, 15, 255);
    //draw_test_pattern();
    draw_moveto(-neg, -neg);
    //*/
    // show_vstcm_splash_screen();
    multicore_launch_core1(draw_loop);
    end_frame();

    read_data(true);
    while (1) {
        int result = read_data(false);
    }
    return 0;
}
 