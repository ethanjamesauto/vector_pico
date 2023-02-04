/**
 * Ethan James (ethanjamesauto)
 * This is a rough port of osresearch/vst for the Raspberry Pi Pico. It's still a work in progress and might not work correctly.
 */

//Original header:
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
 */

// TODO: include C:\\Users\\Ethan\\AppData\\Local\\Arduino15\\packages\\rp2040\\hardware\\rp2040\\2.7.1
#include "hardware/dma.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include <math.h>
#include <stdio.h>

// Size of each buffer
#define BUFFER_SIZE 8192

// Table of values to be sent to DAC
unsigned short DAC_data[2][BUFFER_SIZE];

// Pointer to the address of the DAC data table
unsigned short* address_pointer = &DAC_data[0][0];

// A-channel, 1x, active
#define DAC_A 0b0111000000000000
#define DAC_B 0b1111000000000000

// SPI configurations
#define PIN_CS 5
#define PIN_SCK 6
#define PIN_MOSI 7
#define SPI_PORT spi0

#define FRAME_PIN 8

#define DATA_CHANNEL 0
#define CTRL_CHANNEL 1

// Number of DMA transfers per event
const uint32_t transfer_count = BUFFER_SIZE;

void setup()
{
    pinMode(FRAME_PIN, OUTPUT);
    // Initialize SPI channel
    spi_init(SPI_PORT, 24000000);

    // Format SPI channel (channel, data bits per transfer, polarity, phase,
    // order)
    spi_set_format(SPI_PORT, 16, (spi_cpol_t)0, (spi_cpha_t)0, (spi_order_t)0);

    // Map SPI signals to GPIO ports, acts like framed SPI with this CS mapping
    // gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

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
        BUFFER_SIZE, // Number of transfers
        false // Don't start immediately.
    );

    dma_channel_set_irq0_enabled(CTRL_CHANNEL, true);
    irq_set_exclusive_handler(DMA_IRQ_0, control_complete);
    irq_set_enabled(DMA_IRQ_0, true);

    // start the control channel
    dma_start_channel_mask(1u << CTRL_CHANNEL);
}

#define MAX_PTS 4096
uint32_t point_buffer[2][MAX_PTS];
int point_count[2];
int point_pos = 0;
int buffer_pos = 0;
bool which_point = 0;
bool which_dma = 1; // (which_dma) is read from by the data channel, and (!which_dma) is written to by the line drawing algorithm
static bool done = false; // true if the inactive buffer has been fully prepared and is ready to be sent over SPI

// This function tells the line drawing algorithm to write to the buffer not being read from by the data channel.
// Runs when the control channel has already finished setting up the data channel.
void control_complete()
{
    if (!done) {
        Serial.println("Can't keep up, only reached point " + String(buffer_pos));
    }
    address_pointer = &DAC_data[which_dma][0];
    which_dma = !which_dma;
    done = false;
    dma_hw->ints0 = 1u << CTRL_CHANNEL;
}

void loop()
{
    bool buf = !which_point;
    // Serial.println("Drawing frame with size " + String(point_count[buf]) + ", point 0 is " + String(point_buffer[buf][0]));

    digitalWrite(FRAME_PIN, HIGH);
    bool pin = true;
    for (int i = 0; i < point_count[buf]; i++) {
        if (pin && i > point_count[buf] / 10) {
            digitalWrite(FRAME_PIN, LOW);
            pin = false;
        }
        uint16_t x1 = (point_buffer[buf][i] >> 12) & 0xfff;
        uint16_t y1 = point_buffer[buf][i] & 0xfff;
        uint8_t brightness = (point_buffer[buf][i] >> 24) & 0b111111;
        if (brightness == 0 || i == 0) {
            dwell(x1, y1, 5);
        } else {
            uint16_t x2 = (point_buffer[buf][prev(i, point_count[buf])] >> 12) & 0xfff;
            uint16_t y2 = point_buffer[buf][prev(i, point_count[buf])] & 0xfff;
            lineto(x2, y2, x1, y1);
        }
    }
}

inline int prev(int i, int max)
{
    int prev = i - 1;
    if (prev < 0) {
        prev = max - 1;
    }
    return prev;
}

void dwell(uint16_t x, uint16_t y, uint8_t delay)
{
    for (int i = 0; i < delay; i++) {
        if (i % 2 == 0) {
            DAC_data[!which_dma][buffer_pos++] = DAC_A | x;
        } else {
            DAC_data[!which_dma][buffer_pos++] = DAC_B | y;
        }
        if (buffer_pos >= BUFFER_SIZE) {
            done_buffering();
        }
    }
}

#define DIV 10

// draw a line using breesenham
void lineto(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    x1 /= DIV;
    y1 /= DIV;
    x2 /= DIV;
    y2 /= DIV;

    uint16_t dx = abs(x2 - x1);
    uint16_t dy = abs(y2 - y1);
    int8_t sx = x1 < x2 ? 1 : -1;
    int8_t sy = y1 < y2 ? 1 : -1;
    int32_t err = (dx > dy ? dx : -dy) / 2, e2; // TODO: make int16_t?

    for (;;) {
        if (x1 == x2 && y1 == y2) {
            break;
        }
        e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x1 += sx;
            DAC_data[!which_dma][buffer_pos++] = DAC_A | (x1 * DIV);
            if (buffer_pos >= BUFFER_SIZE) {
                done_buffering();
            }
        }

        if (e2 < dy) {
            err += dx;
            y1 += sy;
            DAC_data[!which_dma][buffer_pos++] = DAC_B | (y1 * DIV);
            if (buffer_pos >= BUFFER_SIZE) {
                done_buffering();
            }
        }
    }
}

// stop writing to the buffer and wait for the data channel to start reading from it
inline void done_buffering()
{
    buffer_pos = 0;
    done = true;
    while (done) {
        delayMicroseconds(20); // TODO: necessary to stop the loop from accessing memory all the time. Rework code so that this isn't necessary.
    };
}

void setup1()
{
    Serial.begin();
    pinMode(9, OUTPUT);
    point_count[1] = 5;
    point_buffer[1][0] = (0 << 24) | (0 << 12) | 0;
    point_buffer[1][1] = (1 << 24) | (4095 << 12) | 0;
    point_buffer[1][2] = (1 << 24) | (4095 << 12) | 4095;
    point_buffer[1][3] = (1 << 24) | (0 << 12) | 4095;
    point_buffer[1][4] = (1 << 24) | (0 << 12) | 0;
}

void loop1()
{

    static uint32_t cmd = 0;
    static char cmd_count = 0;
    static bool waiting = true;

    // read from Serial
    while (Serial.available() > 0) {
        char b = Serial.read();
        if (waiting && b != 0) { // if we receive a nonzero byte, we're ready to start the next frame
            digitalWrite(9, LOW);
            waiting = false;
            point_count[which_point] = 0;
        }
        if (!waiting) {
            cmd = (cmd << 8) | b; // build up a 32-bit command
            if (++cmd_count == 4) { // see if we have a full command
                cmd_count = 0; // reset the command counter
                if (cmd == 0x01010101) { // if we receive a "done" command
                    waiting = true; // go back to waiting
                    digitalWrite(9, HIGH);
                    which_point = !which_point; // switch to the other buffer
                    // while (1)
                    //   ; // halt forever
                } else if (point_count[which_point] < MAX_PTS) {
                    point_buffer[which_point][point_count[which_point]++] = cmd; // add the point to the buffer, then increment the point count
                }
            }
        }
    }
}