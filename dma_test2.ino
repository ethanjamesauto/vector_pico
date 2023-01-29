#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/spi.h"

// SPI configurations
#define PIN_MISO 4
#define PIN_CS 5
#define PIN_SCK 6
#define PIN_MOSI 7
#define SPI_PORT spi0

#define DMA_CH0 0
#define DMA_CH1 1

#define BUFFER_SIZE 512
uint16_t buffer[2][BUFFER_SIZE];

void setup()
{
    Serial.begin(115200);
    // fill buffer
    for (int i = 0; i < BUFFER_SIZE; i++) {
        buffer[0][i] = i;
        buffer[1][i] = i + BUFFER_SIZE;
    }

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

    setup_dma(
        DMA_CH0, DMA_CH1, // DMA channels
        buffer[0], buffer[1], // DMA buffers
        spi_get_hw(SPI_PORT)->dr, // write to the SPI TX FIFO
        0, // buffer size
        dma_complete_isr // interrupt callback
    );

    dma_start_channel_mask(1u << DMA_CH0);
}

void setup_dma(uint ch0, uint ch1, uint16_t* buf0, uint16_t* buf1, io_rw_32 write_addr, size_t buf_size, irq_handler_t isr)
{
    // Setup the data channels
    dma_channel_config c0 = dma_channel_get_default_config(ch0);
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_16); // 16-bit txfers
    channel_config_set_read_increment(&c0, true); // yes read incrementing
    channel_config_set_write_increment(&c0, false); // no write incrementing

    dma_channel_config c1 = dma_channel_get_default_config(ch1);
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_16); // 16-bit txfers
    channel_config_set_read_increment(&c1, true); // yes read incrementing
    channel_config_set_write_increment(&c1, false); // no write incrementing

    // write data at the request of the SPI TX FIFO
    channel_config_set_dreq(&c0, DREQ_SPI0_TX);
    channel_config_set_dreq(&c1, DREQ_SPI0_TX);

    // set the channels to chain to each other
    channel_config_set_chain_to(&c0, ch1);
    channel_config_set_chain_to(&c1, ch0);

    dma_channel_set_irq0_enabled(ch0, true);
    dma_channel_set_irq0_enabled(ch1, true);
    irq_set_exclusive_handler(DMA_IRQ_0, isr);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_configure(
        ch0, // Channel to be configured
        &c0, // The configuration we just created
        &write_addr, // write address
        buf0, // The initial read address
        buf_size, // Number of transfers
        false // Don't start immediately.
    );

    dma_channel_configure(
        ch1, // Channel to be configured
        &c1, // The configuration we just created
        &write_addr, // write address
        buf1, // The initial read address
        buf_size, // Number of transfers
        false // Don't start immediately.
    );
}

void dma_complete_isr()
{
    bool which = dma_hw->ints0 & (1u << DMA_CH1); // which channel just finished? TODO: check for both channels
    if (which) { // prepare DMA channel 1
        dma_hw->ch[DMA_CH1].read_addr = (uint32_t)&buffer[1];
        dma_hw->ints0 = 1u << DMA_CH1; // clear interrupt
    } else { // prepare DMA channel 0
        dma_hw->ch[DMA_CH0].read_addr = (uint32_t)&buffer[0];
    }

    // testing output
    /*
    static bool first = true;
    if (first) {
        first = false;
        Serial.println(which + " just finished");
    }//*/
}

void loop()
{
    // do nothing
}