//
// Created by andrew on 8/9/21.
//

#include <stdio.h>
#include <time.h>
#include "clint.h"
#include "memmap.h"
#include "rv32.h"

#define ACCESS_SIZE 4

#define CLINT_BASE_ADDR 0x2000000
#define CLINT_WINDOW    0x10000

#define MSIP_BASE        (CLINT_BASE_ADDR + 0x0)
#define MSIP_WINDOW      (MSIP_BASE + 0x4)

#define MTIMEL_BASE      (CLINT_BASE_ADDR + 0xbff8)
#define MTIMEL_WINDOW    (MTIMEL_BASE + 0x4)

#define MTIMEH_BASE      (CLINT_BASE_ADDR + 0xbffc)
#define MTIMEH_WINDOW    (MTIMEH_BASE + 0x4)

#define MTIMECMPL_BASE   (CLINT_BASE_ADDR + 0x4000)
#define MTIMECMPL_WINDOW (MTIMECMPL_BASE + 0x4 - 1)

#define MTIMECMPH_BASE   (CLINT_BASE_ADDR + 0x4004)
#define MTIMECMPH_WINDOW (MTIMECMPH_BASE + 0x4)

#define RTC_FREQ 10000000

static bool clint_read_handler(uint32_t address, uint32_t *data, int size);

static bool clint_write_handler(uint32_t address, uint32_t data, int size);

static uint64_t rtc_gettime(void);

static uint64_t mtimecmp = 0;
static uint32_t msip = 0;

void clint_init(void) {
	memmap_append(CLINT_BASE_ADDR, CLINT_BASE_ADDR + CLINT_WINDOW, DEVICE, clint_read_handler, clint_write_handler);
	printf("CLINT inited\r\n");
}

void clint_check_time(void) {
	long delay = 0;
	if (!rv32_get_mip(MIP_MTIP)) {
		delay = mtimecmp - rtc_gettime();

		if (delay < 0) {
			rv32_set_mip(MIP_MTIP);
		}
	}
}

uint64_t clint_mtime(void) {
	return rtc_gettime();
}

static bool clint_read_handler(uint32_t address, uint32_t *data, int size) {
	if (size != ACCESS_SIZE) {
		return false;
	}

	switch (address) {
		case MSIP_BASE ... MSIP_WINDOW - 1:
			*data = msip;
			break;
		case MTIMECMPL_BASE ... MTIMECMPL_WINDOW - 1:
			*data = mtimecmp & 0xffffffff;
			break;
		case MTIMECMPH_BASE ... MTIMECMPH_WINDOW - 1:
			*data = mtimecmp >> 32;
			break;
		case MTIMEL_BASE ... MTIMEL_WINDOW - 1:
			*data = rtc_gettime() & 0xffffffff;
			break;
		case MTIMEH_BASE ... MTIMEH_WINDOW - 1:
			*data = rtc_gettime() >> 32;
			break;
		default:
			return false;
	}

	return true;
}

static bool clint_write_handler(uint32_t address, uint32_t data, int size) {
	if (size != ACCESS_SIZE) {
		return false;
	}

	switch (address) {
		case MSIP_BASE ... MSIP_WINDOW - 1:
			if (data & 1) {
				rv32_set_mip(MIP_MSIP);
			}
			break;
		case MTIMECMPL_BASE ... MTIMECMPL_WINDOW - 1:
			mtimecmp = (mtimecmp & ~0xffffffff) | data;
			rv32_reset_mip(MIP_MTIP);
			break;
		case MTIMECMPH_BASE ... MTIMECMPH_WINDOW - 1:
			mtimecmp = (mtimecmp & 0xffffffff) | ((uint64_t) data << 32);
			rv32_reset_mip(MIP_MTIP);
			break;
		default:
			return false;
	}

	return true;
}

static uint64_t rtc_gettime(void) {
	return rv32_get_instr_cnt();
}
