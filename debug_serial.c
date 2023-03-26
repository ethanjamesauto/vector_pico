/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "debug_serial.h"
#include "hardware/irq.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "settings.h"
#include <stdio.h>

/// \tag::uart_advanced[]

#define UART_ID uart0
#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#define UART_TX_PIN 0
#define UART_RX_PIN 1

// RX interrupt handler
void on_uart_rx()
{
    static char buf[32];
    static int count = 0;
    static enum { WAITING,
        PARSING } state
        = WAITING;

    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);
        // if (uart_is_writable(UART_ID)) { uart_putc(UART_ID, ch);}
        switch (state) {
        case WAITING:
            if (ch >= '0' && ch <= '9') {
                state = PARSING;
            } else {
                break;
            }
        case PARSING:
            if (ch == '\n') {
                buf[count] = '\0';
                if (uart_is_writable(UART_ID)) {
                    int setting, value;
                    // scan two ints into a and b
                    sscanf(buf, "%d %d", &setting, &value);
                    uart_puts(UART_ID, "Updated setting ");
                    uart_putc(UART_ID, (char)setting + '0');
                    uart_putc(UART_ID, '\n');

                    update_setting(setting, value);
                }

                state = WAITING;
                count = 0;
            }

            buf[count++] = ch;
            if (count == sizeof(buf)) { // command is too long
                state == WAITING;
                count = 0;
            }
            break;
        }
    }
}

void debug_send(char* buf)
{
#ifdef ENABLE_DEBUG_SERIAL
    uart_puts(UART_ID, buf);
#endif
}

void debug_serial_init()
{
#ifdef ENABLE_DEBUG_SERIAL
    // Set up our UART with a basic baud rate.
    uart_init(UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Actually, we want a different speed
    // The call will return the actual baud rate selected, which will be as close as
    // possible to that requested
    int __unused actual = uart_set_baudrate(UART_ID, BAUD_RATE);

    // Set UART flow control CTS/RTS, we don't want these, so turn them off
    uart_set_hw_flow(UART_ID, false, false);

    // Set our data format
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(UART_ID, false);

    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

    // And set up and enable the interrupt handlers
    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(UART_ID, true, false);

    // OK, all set up.
    // Lets send a basic string out, and then run a loop and wait for RX interrupts
    // The handler will count them, but also reflect the incoming data back with a slight change!
    uart_puts(UART_ID, "Initiated serial connection\n");
#endif
}

/// \end:uart_advanced[]