//
// Created by andrew on 7/25/21.
//

#ifndef YARISCVEMU_RV32_H
#define YARISCVEMU_RV32_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
	MIP_SSIP = (1 << 1),
	MIP_MSIP = (1 << 3),
	MIP_STIP = (1 << 5),
	MIP_MTIP = (1 << 7),
	MIP_SEIP = (1 << 9),
	MIP_MEIP = (1 << 11)
} mip_flags;


typedef struct rv32_state_s rv32_state;

void rv32_init(void);

void rv32_reset_mip(mip_flags fl);

void rv32_set_mip(mip_flags fl);

bool rv32_get_mip(mip_flags fl);

uint64_t rv32_get_instr_cnt();

void rv32_raise_interrupts(void);

void rv32_run(uint64_t cycles);

#endif //YARISCVEMU_RV32_H
