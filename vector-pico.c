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
    //  while (2)
    //    puts("Hello, world!");//*/
    /*
    int sm = 0;
    uint offset = pio_add_program(pio0, &spi_cpha0_cs_program);
    pio_spi_cs_init(pio0, sm, offset, 24, 133e6 / (50e6 * 2), 5, 7);

    static uint16_t i = 0;
    while (1) {
        //pio_send(DAC_X | ((i += 128) & 0xfff), pio0, sm);

        uint val = max5716_data_word(i);
        i += 2048;
        pio_sm_put_blocking(pio0, sm, val);
        for (volatile int i = 0; i < 1000; i++);
    }*/

    init();
    begin_frame();

    const int pos = 2047;
    const int neg = 2048;
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
