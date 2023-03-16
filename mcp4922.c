// channel bitmasks for MCP4922 DAC
#define DAC_X 0b0111000000000000
#define DAC_Y 0b1111000000000000

void pio_send(uint16_t data, PIO pio, uint sm)
{
    static bool odd = false;
    static int word;

    if (odd)
        pio_sm_put_blocking(pio, sm, word | data);
    else
        word = data << 16;

    odd = !odd;
}