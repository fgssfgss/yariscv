//
// Created by andrew on 9/5/21.
//

#include <malloc.h>
#include <string.h>
#include "tlb.h"
#include "logger.h"

/* DUMB TLB CACHE IMPLEMENTATION
 * Uses ~8 MBytes of RAM
 * maps virtual address -> physical address
 * have a field to mark access type
 * 20 bits virt -> 20 bits phys */
/* TODO: write an LRU TLB cache */

#define SIZE ((1 << 20) - 1)
#define PG_SHIFT 12
#define WR 1
#define RE 2

typedef struct {
	uint32_t physical_address;
	int access;
} tlb_entry;

static tlb_entry *entries;

void tlb_init(void) {
	entries = (tlb_entry *) calloc(SIZE, sizeof(tlb_entry));
	printf("TLB cache inited! size %d\n", SIZE);
}

bool tlb_get(uint32_t v_address, uint32_t *p_address, access_t operation) {
	int acc = (operation == WRITE) ? WR : RE;
	uint32_t shift = v_address & 0xfff;
	v_address >>= PG_SHIFT;

	tlb_entry ent = entries[v_address];

	logger("tlb_get entry at 0x%08x = access %d, phys_addr 0x%08x\n", v_address, ent.access, ent.physical_address);

	if (ent.access & acc) {
		*p_address = (ent.physical_address << PG_SHIFT) | (shift & 0xfff);
		return true;
	} else {
		return false;
	}
}

void tlb_put(uint32_t v_address, uint32_t p_address, access_t operation) {
	v_address >>= PG_SHIFT;

	tlb_entry ent = entries[v_address];

	ent.access |= (operation == WRITE) ? WR : RE;
	ent.physical_address = (p_address >> PG_SHIFT);

	logger("tlb_put entry at 0x%08x = access %d, phys_addr 0x%08x\n", v_address, ent.access, ent.physical_address);
	entries[v_address] = ent;
}

void tlb_flush(void) {
	memset(entries, 0x0, SIZE * sizeof(tlb_entry));
}