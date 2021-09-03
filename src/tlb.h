//
// Created by andrew on 9/5/21.
//

#ifndef YARISCVEMU_TLB_H
#define YARISCVEMU_TLB_H

#include <stdint.h>
#include <stdbool.h>
#include "mmu.h"

void tlb_init(void);

bool tlb_get(uint32_t v_address, uint32_t *p_address, access_t operation);

void tlb_put(uint32_t v_address, uint32_t p_address, access_t operation);

void tlb_flush(void);

#endif //YARISCVEMU_TLB_H
