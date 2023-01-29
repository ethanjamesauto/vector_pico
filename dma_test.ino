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

// Size of each buffer
#define buffer_size 256

// Table of values to be sent to DAC
unsigned short DAC_data[2][buffer_size];

// Pointer to the address of the DAC data table
unsigned short* address_pointer = &DAC_data[0][0];

// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000

// SPI configurations
#define PIN_MISO 4
#define PIN_CS 5
#define PIN_SCK 6
#define PIN_MOSI 7
#define SPI_PORT spi0

#define DATA_CHANNEL 0
#define CTRL_CHANNEL 1

// Number of DMA transfers per event
const uint32_t transfer_count = buffer_size;

void setup()
{
    // Initialize SPI channel (channel, baud rate set to 20MHz)
    spi_init(SPI_PORT, 24000000);

    // Format SPI channel (channel, data bits per transfer, polarity, phase,
    // order)
    spi_set_format(SPI_PORT, 16, (spi_cpol_t)0, (spi_cpha_t)0, (spi_order_t)0);

    // Map SPI signals to GPIO ports, acts like framed SPI with this CS mapping
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    int i;
    for (i = 0; i < (buffer_size); i++) {
        uint16_t data = i * 2;
        DAC_data[0][i] = DAC_config_chan_A | (data & 0x0fff);
        DAC_data[1][i] = DAC_config_chan_A | ((data + 1) & 0x0fff);
    }

    // Set up the control channel
    dma_channel_config c = dma_channel_get_default_config(CTRL_CHANNEL); // default configs
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32); // 32-bit txfers
    channel_config_set_read_increment(&c, false); // no read incrementing
    channel_config_set_write_increment(&c, false); // no write incrementing
    channel_config_set_chain_to(&c, DATA_CHANNEL); // chain to data channel

    dma_channel_configure(
        CTRL_CHANNEL, // Channel to be configured
        &c, // The configuration we just created
        &dma_hw->ch[DATA_CHANNEL]
             .read_addr, // Write address (data channel read address)
        &address_pointer, // Read address (POINTER TO AN ADDRESS)
        1, // Number of transfers
        false // Don't start immediately
    );

    // Set up the data channel
    dma_channel_config c2 = dma_channel_get_default_config(DATA_CHANNEL); // Default configs
    channel_config_set_transfer_data_size(&c2, DMA_SIZE_16); // 16-bit txfers
    channel_config_set_read_increment(&c2, true); // yes read incrementing
    channel_config_set_write_increment(&c2, false); // no write incrementing
    // write data at the request of the SPI TX FIFO
    channel_config_set_dreq(&c2, DREQ_SPI0_TX);

    // chain to the controller DMA channel
    channel_config_set_chain_to(&c2, CTRL_CHANNEL); // Chain to control channel

    dma_channel_configure(
        DATA_CHANNEL, // Channel to be configured
        &c2, // The configuration we just created
        &spi_get_hw(SPI_PORT)->dr, // write address (SPI data register)
        DAC_data[0], // The initial read address
        buffer_size, // Number of transfers
        false // Don't start immediately.
    );

    dma_channel_set_irq0_enabled(CTRL_CHANNEL, true);
    irq_set_exclusive_handler(DMA_IRQ_0, isr);
    irq_set_enabled(DMA_IRQ_0, true);

    // start the control channel
    dma_start_channel_mask(1u << CTRL_CHANNEL);
}

bool which = 1;
bool done = true;

void isr()
{
    if (!done) {
        Serial.println("Can't keep up!");
    } else {
        address_pointer = &DAC_data[which][0];
        which = !which;
        done = false;
    }
    dma_hw->ints0 = 1u << CTRL_CHANNEL;
}

void loop()
{
    static uint16_t i = 0;
    // set every item in DAC_data[!which] to i
    if (!done) {
        for (int j = 0; j < buffer_size; j++) {
            DAC_data[!which][j] = i % 8; // DAC_config_chan_A | (i & 0x0fff);
        }
        done = true;
        i++;
    }
}

void setup1()
{
    Serial.begin();
}

void loop1()
{
    while (Serial.available() > 0) {
        Serial.write(Serial.read());
    }
}