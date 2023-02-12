#include "hardware/dma.h"
#include "hardware/spi.h"

#define DATA_CHANNEL 0
#define CTRL_CHANNEL 1

// Size of each buffer
#define BUFFER_SIZE 512

// Number of DMA transfers per event
const uint32_t transfer_count = BUFFER_SIZE;

// Table of values to be sent to DAC
unsigned short DAC_data[2][BUFFER_SIZE];

// Pointer to the address of the DAC data table
unsigned short* address_pointer = &DAC_data[0][0];

// bit masks for the 16-bit SPI dac control words
#define DAC_A 0b0111000000000000
#define DAC_B 0b1111000000000000

// SPI configurations
#define PIN_CS 5
#define PIN_SCK 6
#define PIN_MOSI 7
#define SPI_PORT spi0

bool which_dma = 1; // (which_dma) is read from by the data channel, and (!which_dma) is written to by the line drawing algorithm
static bool done = false; // true if the inactive buffer has been fully prepared and is ready to be sent over SPI
int buffer_pos = 0;

// This interrupt service routine tells the line drawing algorithm to write to the buffer not being read from by the data channel.
// Runs when the control channel has already finished setting up the data channel.
void control_complete_isr()
{
    // if (!done)
    //     Serial.println("Can't keep up, only reached point " + String(buffer_pos));
    which_dma = !which_dma;
    // buffer_pos = 0;
    done = false;
    address_pointer = &DAC_data[!which_dma][0];
    dma_hw->ints0 = 1u << CTRL_CHANNEL;
}

inline void spi_dma_init()
{
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
    irq_set_exclusive_handler(DMA_IRQ_0, control_complete_isr);
    irq_set_enabled(DMA_IRQ_0, true);

    // start the control channel
    dma_start_channel_mask(1u << CTRL_CHANNEL);
}

//wait until the dma transfer is done before writing to the next buffer
inline void done_buffering()
{
    buffer_pos = 0;
    done = true;
    while (done) {
        // TODO: this is necessary to stop the loop from accessing memory all of the time.
        // Rework code so that this isn't necessary.
        delayMicroseconds(20);
    }
}