#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "build/spi_half_duplex.pio.h"

#include "drawing.h"
#include "max5716.h"

#include <stdio.h>

int main()
{
    stdio_init_all();
    //  while (2)
    //    puts("Hello, world!");//*/

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
    }

    return 0;
}
