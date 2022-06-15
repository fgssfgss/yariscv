//
// Created by andrew on 7/28/21.
//

#ifndef YARISCVEMU_MEMMAP_H
#define YARISCVEMU_MEMMAP_H

#include <stdint.h>
#include <stdbool.h>

typedef bool (*read_func_t)(uint32_t address, uint32_t *data, int size);

typedef bool (*write_func_t)(uint32_t address, uint32_t data, int size);

typedef enum {
	RAM,
	DEVICE
} memtype;

typedef struct memmap_entry_s {
	uint32_t start;
	uint32_t size;

	memtype type;

	/* HANDLE READ/WRITE FUNCTIONS */
	union {
		struct {
			read_func_t read_handler;
			write_func_t write_handler;
		};
		uint8_t *phys_mem;
	};
} memmap_entry_t;

void memmap_init(void);

void memmap_append(uint32_t start, uint32_t end, memtype type, read_func_t read, write_func_t write);

memmap_entry_t *memmap_get(uint32_t address);

#endif //YARISCVEMU_MEMMAP_H
