//
// Created by andrew on 8/2/21.
//

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "loader.h"

static uint64_t loader_get_filesize(FILE *fp);

bool loader_load(const char *filename, uint32_t address) {
	memmap_entry_t *out = memmap_get(address);
	uintptr_t offset = 0;

	if (out == NULL || out->type != RAM) {
		return false;
	}

	offset = address - out->start;

	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		return false;
	}

	uint64_t sz = loader_get_filesize(fp);
	if (sz == 0 || sz > out->size) {
		return false;
	}

	printf("Loading '%s' at 0x%08lx, size is %08lx\n", filename, offset + out->start, sz);

	if (fread((void *) (out->phys_mem + offset), sizeof(char), sz, fp) != (sz * sizeof(char))) {
		printf("Failed to write to the memory!\n");
		return false;
	}

	return true;
}

static uint64_t loader_get_filesize(FILE *fp) {
	uint64_t sz = 0;
	fseek(fp, 0L, SEEK_END);
	sz = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	return sz;
}

