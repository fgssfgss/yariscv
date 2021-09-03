//
// Created by andrew on 8/7/21.
//

#include <stdio.h>
#include "plic.h"
#include "memmap.h"
#include "logger.h"

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

static uint32_t plic_read_handler(uint32_t address, int size);
static void plic_write_handler(uint32_t address, uint32_t data, int size);

void plic_init(void) {
    memmap_append(PLIC_BASE_ADDR, PLIC_BASE_ADDR + PLIC_WINDOW, DEVICE, plic_read_handler, plic_write_handler);
    printf("PLIC inited\r\n");
}

static uint32_t plic_read_handler(uint32_t address, int size) {
    switch(address) {
    	case SOURCE_PRIORITY ... SOURCE_PRIORITY_WINDOW - 1: {
    		int index = (address - SOURCE_PRIORITY) / 4;
    		return read_unaligned_reg(address - SOURCE_PRIORITY, priorities[index], size);
    	}
    	case PENDING ... PENDING_WINDOW - 1: {
    		int index = (address - PENDING) / 4;
    		return read_unaligned_reg(address - PENDING, pending[index], size);
    	}
    	case ENABLE ... ENABLE_WINDOW - 1: {
    		int index = (address - ENABLE) / 4;
    		return read_unaligned_reg(address - ENABLE, enabled[index], size);
    	}
    	case THRESHOLD_AND_CLAIM ... THRESHOLD_AND_CLAIM_WINDOW - 1: {
    		switch(address - THRESHOLD_AND_CLAIM) {
    			case 0x4:
    				return threshold[0];
    			case 0x8:
    				return claim[0];
    			case 0x1000:
    				return threshold[1];
    			case 0x1004:
    				return claim[1];
    			default:
    				return 0;
    		}
    		return 0;
    	}
	    default:
	    	return 0;
    }
}

static void plic_write_handler(uint32_t address, uint32_t data, int size) {
	switch(address) {
		case SOURCE_PRIORITY ... SOURCE_PRIORITY_WINDOW - 1: {
			uint32_t index = (address - SOURCE_PRIORITY) / 4;
			write_unaligned_reg(address - SOURCE_PRIORITY, data, &priorities[index], size);
			return;
		}
		case PENDING ... PENDING_WINDOW - 1: {
			uint32_t index = (address - PENDING) / 4;
			write_unaligned_reg(address - PENDING, data, &pending[index], size);
			return;
		}
		case ENABLE ... ENABLE_WINDOW - 1: {
			uint32_t index = (address - ENABLE) / 4;
			write_unaligned_reg(address - ENABLE, data, &enabled[index], size);
			return;
		}
		case THRESHOLD_AND_CLAIM ... THRESHOLD_AND_CLAIM_WINDOW - 1: {
			switch(address - THRESHOLD_AND_CLAIM) {
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
			return;
		}
		default: {
			// noop
		}
	}
}