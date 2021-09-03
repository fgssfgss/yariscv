//
// Created by andrew on 7/28/21.
//

#include "memmap.h"
#include <malloc.h>
#include <string.h>

#define MAX_ENTRY 10
static memmap_entry_t **entries;
static long position = 0;

void memmap_init(void) {
	entries = (memmap_entry_t **) malloc(MAX_ENTRY * sizeof(memmap_entry_t *));
	memset(entries, 0x0, MAX_ENTRY * sizeof(memmap_entry_t *));
}

void memmap_append(uint32_t start, uint32_t end, memtype type, read_func_t read, write_func_t write) {
	memmap_entry_t *entry = (memmap_entry_t *) calloc(1, sizeof(*entry));

	// TODO: some checks against NULL pointer and arg validity ??

	entry->start = start;
	entry->end = end;
	entry->type = type;

	if (type != RAM) {
		entry->read_handler = read;
		entry->write_handler = write;
	} else {
		entry->phys_mem = (uint8_t *) calloc(1, sizeof(uint8_t) * (entry->end - entry->start));
	}

	// TODO: maybe sort it and do binary search??
	entries[position] = entry;
	position++;
}

memmap_entry_t *memmap_get(uint32_t address) {
	for (long i = 0; i < MAX_ENTRY; ++i) {
		memmap_entry_t *entry = entries[i];
		if (entry != NULL && entry->start <= address && entry->end >= address) {
			return entry;
		}
	}

	return NULL;
}
