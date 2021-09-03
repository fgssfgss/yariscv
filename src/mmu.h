//
// Created by andrew on 7/27/21.
//

#ifndef YARISCVEMU_MMU_H
#define YARISCVEMU_MMU_H

#include <stdint.h>
#include <stdbool.h>
#include "rv32.h"

typedef enum {
	READ,
	WRITE,
	EXECUTE
} access_t;

typedef enum {
	MMU_OK,
	MMU_ACCESS_FAULT,
	MMU_PAGE_FAULT
} mmu_error_t;

void mmu_init(rv32_state *state);

mmu_error_t mmu_exec_u32(uint32_t address, uint32_t *data);

mmu_error_t mmu_read_u8(uint32_t address, uint8_t *data);

mmu_error_t mmu_read_u16(uint32_t address, uint16_t *data);

mmu_error_t mmu_read_u32(uint32_t address, uint32_t *data);

mmu_error_t mmu_write_u8(uint32_t address, uint8_t data);

mmu_error_t mmu_write_u16(uint32_t address, uint16_t data);

mmu_error_t mmu_write_u32(uint32_t address, uint32_t data);


#endif //YARISCVEMU_MMU_H
