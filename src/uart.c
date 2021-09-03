//
// Created by andrew on 8/2/21.
//

#include <stdio.h>
#include "uart.h"
#include "memmap.h"
#include "console.h"

// LiteX UART Implementation

#define BASE_ADDR      0x01000000
#define REG_RXTX       0
#define REG_TXFULL     4
#define REG_RXEMPTY    8
#define REG_EV_STATUS  12
#define REG_EV_PENDING 16
#define REG_EV_ENABLE  20

static bool uart_read_handler(uint32_t address, uint32_t *data, int size);

static bool uart_write_handler(uint32_t address, uint32_t data, int size);

void uart_init(void) {
	memmap_append(BASE_ADDR, BASE_ADDR + 0x18, DEVICE, uart_read_handler, uart_write_handler);
	printf("LiteX UART inited\r\n");
}

static bool uart_read_handler(uint32_t address, uint32_t *data, int size) {
	if (size != 4) {
		return false;
	}

	switch (address) {
		case BASE_ADDR + REG_RXTX:
			*data = console_get_char() & 0xff;
			break;
		case BASE_ADDR + REG_TXFULL:
			*data = 0;
			fflush(stdout);
			break;
		case BASE_ADDR + REG_RXEMPTY:
			*data = console_is_empty();
			break;
		default:
			*data = 0;
			break;
	}

	return true;
}

static bool uart_write_handler(uint32_t address, uint32_t data, int size) {
	uint32_t val = data;

	if (size != 4) {
		return false;
	}

	switch (address) {
		case BASE_ADDR + REG_RXTX:
			putc(val, stdout);
			break;
		case BASE_ADDR + REG_EV_PENDING:
			break;
		case BASE_ADDR + REG_EV_ENABLE:
			break;
		default:
			break;
	}

	return true;
}

