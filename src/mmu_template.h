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
    if (ent == NULL) {
        return MMU_ACCESS_FAULT;
    }

    if (ent->type == RAM) {
        *data = *(T *)(ent->phys_mem + (uintptr_t)(p_address - ent->start));
    } else {
        *data = ent->read_handler(p_address, sizeof(T));
    }

    return MMU_OK;
}

static mmu_error_t TEMPLATE(mem_write, T)(uint32_t p_address, T data) {
    memmap_entry_t *ent = memmap_get(p_address);
    if (ent == NULL) {
        return MMU_ACCESS_FAULT;
    }

    if (p_address >= 0x81888000 && p_address < 0x81889000) {
    	logger("shiiit written 0x%08x to 0x%08x\n", data, p_address);
    }

    if (ent->type == RAM) {
        *(T *)(ent->phys_mem + (uintptr_t)(p_address - ent->start)) = data;
    } else {
        ent->write_handler(p_address, data, sizeof(T));
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
}

#endif
