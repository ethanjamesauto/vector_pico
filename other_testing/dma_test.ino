/**
 *  V. Hunter Adams (vha3)
                Code based on examples from Raspberry Pi Co
                Sets up two DMA channels. One sends samples at audio rate to
 the DAC, (data_chan), and the other writes to the data_chan DMA control
 registers (ctrl_chan). The control channel writes to the data channel, sending
 one period of a sine wave thru the DAC. The control channel is chained to the
 data channel, so it is re-triggered after the data channel is finished. The
 data channel is chained to the control channel, so it is restarted as soon as
 the control channel finishes. The data channel is paced by a timer to perform
 transactions at audio rate.

                Edited 1/28/2023 by Ethan James
 */

// TODO: include C:\\Users\\Ethan\\AppData\\Local\\Arduino15\\packages\\rp2040\\hardware\\rp2040\\2.7.1
#include "hardware/dma.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include <math.h>
#include <stdio.h>

// Number of samples per period in sine table
#define sine_table_size 256

// Sine table
int raw_sin[sine_table_size];

// Table of values to be sent to DAC
unsigned short DAC_data[sine_table_size];

// Pointer to the address of the DAC data table
unsigned short* address_pointer = &DAC_data[0];

// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000

// SPI configurations
#define PIN_MISO 4
#define PIN_CS 5
#define PIN_SCK 6
#define PIN_MOSI 7
#define SPI_PORT spi0

// Number of DMA transfers per event
const uint32_t transfer_count = sine_table_size;

void setup()
{
    // Initidalize stdio
    // stdio_init_all();

    // Initialize SPI channel (channel, baud rate set to 20MHz)
    spi_init(SPI_PORT, 20000000);

    // Format SPI channel (channel, data bits per transfer, polarity, phase,
    // order)
    spi_set_format(SPI_PORT, 16, (spi_cpol_t)0, (spi_cpha_t)0, (spi_order_t)0);

    // Map SPI signals to GPIO ports, acts like framed SPI with this CS mapping
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // Build sine table and DAC data table
    int i;
    for (i = 0; i < (sine_table_size); i++) {
        raw_sin[i] = (int)(2047 * sin((float)i * 6.283 / (float)sine_table_size) + 2047); // 12 bit
        DAC_data[i] = DAC_config_chan_A | (raw_sin[i] & 0x0fff);
    }

    // Select DMA channels
    int data_chan = 0;
    int ctrl_chan = 1;

    // Setup the control channel
    dma_channel_config c = dma_channel_get_default_config(ctrl_chan); // default configs
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32); // 32-bit txfers
    channel_config_set_read_increment(&c, false); // no read incrementing
    channel_config_set_write_increment(&c, false); // no write incrementing
    channel_config_set_chain_to(&c, data_chan); // chain to data channel

    dma_channel_configure(
        ctrl_chan, // Channel to be configured
        &c, // The configuration we just created
        &dma_hw->ch[data_chan]
             .read_addr, // Write address (data channel read address)
        &address_pointer, // Read address (POINTER TO AN ADDRESS)
        1, // Number of transfers
        false // Don't start immediately
    );

    // Setup the data channel
    dma_channel_config c2 = dma_channel_get_default_config(data_chan); // Default configs
    channel_config_set_transfer_data_size(&c2, DMA_SIZE_16); // 16-bit txfers
    channel_config_set_read_increment(&c2, true); // yes read incrementing
    channel_config_set_write_increment(&c2, false); // no write incrementing
    // (X/Y)*sys_clk, where X is the first 16 bytes and Y is the second
    // sys_clk is 125 MHz unless changed in code. Configured to ~44 kHz
    dma_timer_set_fraction(0, 0x0017, 0xffff);
    // 0x3b means timer0 (see SDK manual)
    // channel_config_set_dreq(&c2, 0x3b); // DREQ paced by timer 0
    // write data at the request of the SPI TX FIFO
    channel_config_set_dreq(&c2, DREQ_SPI0_TX);
	
    // chain to the controller DMA channel
    channel_config_set_chain_to(&c2, ctrl_chan); // Chain to control channel

    dma_channel_configure(
        data_chan, // Channel to be configured
        &c2, // The configuration we just created
        &spi_get_hw(SPI_PORT)->dr, // write address (SPI data register)
        DAC_data, // The initial read address
        sine_table_size, // Number of transfers
        false // Don't start immediately.
    );

    // start the control channel
    dma_start_channel_mask(1u << ctrl_chan);
}

void loop()
{
    // do nothing
}