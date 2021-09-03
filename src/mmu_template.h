//
// Created by andrew on 8/2/21.
//

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "logger.h"

#ifdef T

static mmu_error_t TEMPLATE(mem_read, T)(uint32_t p_address, T *data) {
	memmap_entry_t *ent = memmap_get(p_address);
	uint32_t u32_data;
	if (ent == NULL) {
		return MMU_ACCESS_FAULT;
	}

	if (ent->type == RAM) {
		*data = *(T *) (ent->phys_mem + (uintptr_t) (p_address - ent->start));
	} else {
		if (!ent->read_handler(p_address, &u32_data, sizeof(T))) {
			return MMU_ACCESS_FAULT;
		}
		*data = (T) u32_data;
	}

	return MMU_OK;
}

static mmu_error_t TEMPLATE(mem_write, T)(uint32_t p_address, T data) {
	memmap_entry_t *ent = memmap_get(p_address);
	if (ent == NULL) {
		return MMU_ACCESS_FAULT;
	}

	if (ent->type == RAM) {
		*(T *) (ent->phys_mem + (uintptr_t) (p_address - ent->start)) = data;
	} else {
		if (!ent->write_handler(p_address, data, sizeof(T))) {
			return MMU_ACCESS_FAULT;
		}
	}

	return MMU_OK;
}

mmu_error_t TEMPLATE(mmu_read, T)(uint32_t address, T *data) {
	uint32_t p_address = 0;
	mmu_error_t res = MMU_OK;
	if ((res = translate_address(address, &p_address, READ)) != MMU_OK) {
		return res;
	}
	if ((res = TEMPLATE(mem_read, T)(p_address, data)) != MMU_OK) {
		return res;
	}
	return res;
}

mmu_error_t TEMPLATE(mmu_write, T)(uint32_t address, T data) {
	uint32_t p_address = 0;
	mmu_error_t res = MMU_OK;
	if ((res = translate_address(address, &p_address, WRITE)) != MMU_OK) {
		return res;
	}
	if ((res = TEMPLATE(mem_write, T)(p_address, data)) != MMU_OK) {
		return res;
	}
	return res;
}

#endif
