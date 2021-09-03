//
// Created by andrew on 8/7/21.
//

#include <stdio.h>
#include "plic.h"
#include "memmap.h"
#include "logger.h"

// FIXME: whole file is a stub to run Linux

#define ACCESS_SIZE 4

#define PLIC_BASE_ADDR 0xc000000
#define PLIC_WINDOW    0x4000000

#define SOURCE_PRIORITY        PLIC_BASE_ADDR
#define SOURCE_PRIORITY_WINDOW (PLIC_BASE_ADDR + 0x1000)

#define PENDING                (PLIC_BASE_ADDR + 0x1000)
#define PENDING_WINDOW         (PLIC_BASE_ADDR + 0x1080)

#define ENABLE                 (PLIC_BASE_ADDR + 0x2000)
#define ENABLE_WINDOW          (PLIC_BASE_ADDR + 0x2100)

#define THRESHOLD_AND_CLAIM        (PLIC_BASE_ADDR + 0x200000)
#define THRESHOLD_AND_CLAIM_WINDOW (PLIC_BASE_ADDR + 0x201008)

#define SOURCE_NUM (1024)

static uint32_t priorities[SOURCE_NUM];
static uint32_t enabled[64];
static uint32_t pending[32];
static uint32_t threshold[2];
static uint32_t claim[2];

static bool plic_read_handler(uint32_t address, uint32_t *data, int size);

static bool plic_write_handler(uint32_t address, uint32_t data, int size);

void plic_init(void) {
	memmap_append(PLIC_BASE_ADDR, PLIC_BASE_ADDR + PLIC_WINDOW, DEVICE, plic_read_handler, plic_write_handler);
	printf("PLIC inited\r\n");
}

static bool plic_read_handler(uint32_t address, uint32_t *data, int size) {
	uint32_t index = 0;

	if (size != ACCESS_SIZE) {
		return false;
	}

	switch (address) {
		case SOURCE_PRIORITY ... SOURCE_PRIORITY_WINDOW - 1:
			index = (address - SOURCE_PRIORITY) / 4;
			*data = priorities[index];
			break;
		case PENDING ... PENDING_WINDOW - 1:
			index = (address - PENDING) / 4;
			*data = pending[index];
			break;
		case ENABLE ... ENABLE_WINDOW - 1:
			index = (address - ENABLE) / 4;
			*data = enabled[index];
			break;
		case THRESHOLD_AND_CLAIM ... THRESHOLD_AND_CLAIM_WINDOW - 1: {
			switch (address - THRESHOLD_AND_CLAIM) {
				case 0x4:
					*data = threshold[0];
					break;
				case 0x8:
					*data = claim[0];
					break;
				case 0x1000:
					*data = threshold[1];
					break;
				case 0x1004:
					*data = claim[1];
					break;
				default:
					*data = 0;
					break;
			}
			break;
		}
		default:
			*data = 0;
			break;
	}

	return true;
}

static bool plic_write_handler(uint32_t address, uint32_t data, int size) {
	uint32_t index = 0;
	if (size != ACCESS_SIZE) {
		return false;
	}

	switch (address) {
		case SOURCE_PRIORITY ... SOURCE_PRIORITY_WINDOW - 1:
			index = (address - SOURCE_PRIORITY) / 4;
			priorities[index] = data;
			break;
		case PENDING ... PENDING_WINDOW - 1:
			index = (address - PENDING) / 4;
			pending[index] = data;
			break;
		case ENABLE ... ENABLE_WINDOW - 1:
			index = (address - ENABLE) / 4;
			enabled[index] = data;
			break;
		case THRESHOLD_AND_CLAIM ... THRESHOLD_AND_CLAIM_WINDOW - 1: {
			switch (address - THRESHOLD_AND_CLAIM) {
				case 0x4:
					threshold[0] = data;
					break;
				case 0x8:
					claim[0] = data;
					break;
				case 0x1000:
					threshold[1] = data;
					break;
				case 0x1004:
					claim[1] = data;
					break;
				default:
					break;
			}
			break;
		}
		default:
			break;
	}

	return true;
}