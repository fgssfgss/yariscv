//
// Created by andrew on 8/2/21.
//

#include <stdio.h>
#include "uart.h"
#include "memmap.h"

// LiteX UART Implementation

#define BASE_ADDR      0x01000000
#define REG_RXTX       0
#define REG_TXFULL     4
#define REG_RXEMPTY    8
#define REG_EV_STATUS  12
#define REG_EV_PENDING 16
#define REG_EV_ENABLE  20

static uint32_t uart_read_handler(uint32_t address, int size);
static void uart_write_handler(uint32_t address, uint32_t data, int size);

void uart_init(void) {
    memmap_append(BASE_ADDR, BASE_ADDR + 0x18, DEVICE, uart_read_handler, uart_write_handler);
    printf("LiteX UART inited\r\n");
}

static uint32_t uart_read_handler(uint32_t address, int size) {
    switch(address) {
        case BASE_ADDR + REG_RXTX:
            return 0;
        case BASE_ADDR + REG_TXFULL:
            return 0;
        case BASE_ADDR + REG_RXEMPTY:
            return 0x1; // empty RX buffer
        default:
            return 0;
    }
}

static void uart_write_handler(uint32_t address, uint32_t data, int size) {
    uint32_t val = data;
    switch(address) {
        case BASE_ADDR + REG_RXTX:
            putc(val, stdout);
            fflush(stdout);
            break;
        case BASE_ADDR + REG_EV_PENDING:
            break;
        default:
            break;
    }
}