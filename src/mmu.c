//
// Created by andrew on 7/27/21.
//

#include "mmu.h"
#include "rv32_p.h"
#include "memmap.h"

#define PG_SHIFT 12
#define LEVELS 2
#define PTE_SIZE 4
#define PN_MASK 0x3ff
#define PTE_V_MASK (1 << 0)
#define PTE_R_MASK (1 << 1)
#define PTE_W_MASK (1 << 2)
#define PTE_X_MASK (1 << 3)
#define PTE_U_MASK (1 << 4)
#define PTE_A_MASK (1 << 6)
#define PTE_D_MASK (1 << 7)

// we need it to be able to access satp register and others
static rv32_state *s;
static inline mmu_error_t translate_address(uint32_t v_address, uint32_t *p_address, access_t access);
static inline mmu_error_t translate_sv32(uint32_t v_address, uint32_t *p_address, access_t access, int priv);

// TEMPLATE MAGIC
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#define CAT(X,Y) X##_##Y
#define TEMPLATE(X,Y) CAT(X,Y)
#define T u8
#include "mmu_template.h"
#undef T
#define T u16
#include "mmu_template.h"
#undef T
#define T u32
#include "mmu_template.h"
#include "logger.h"
#undef T

mmu_error_t TEMPLATE(mmu_exec, u32)(uint32_t address, u32 *data) {
    uint32_t p_address = 0;
    mmu_error_t res = MMU_OK;
    if ((res = translate_address(address, &p_address, EXECUTE)) != MMU_OK) {
        return res;
    }
    if ((res = TEMPLATE(mem_read, u32)(p_address, data)) != MMU_OK) {
        return res;
    }
}


void mmu_init(rv32_state *state) {
    s = state;
}

static inline mmu_error_t translate_address(uint32_t v_address, uint32_t *p_address, access_t access) {
    int priv;

    if (s->mstatus_mprv && access != EXECUTE) {
        /* use previous privilege */
        priv = s->mstatus_mpp;
    } else {
        priv = s->prv;
    }

    if ((s->satp >> 31) == 1 && (priv != 3)) {
    	logger("SV32 triggered, priv is %d, mpp is %d, mprv is %d\n", priv, s->mstatus_mpp, s->mstatus_mprv);
        /* sv32 mode */
        return translate_sv32(v_address, p_address, access, priv);
    } else {
    	*p_address = v_address;
    	return MMU_OK;
    }
}

static inline mmu_error_t translate_sv32(uint32_t v_address, uint32_t *p_address, access_t access, int priv) {
	uint32_t pte = 0;
	uint64_t pte_addr = 0;
	uint32_t xwr = 0;
	bool need_write;
	uint64_t a = s->satp << PG_SHIFT;
	uint64_t p_addr = 0;
	int i = LEVELS - 1;

	while(i >= 0) {
		pte_addr = a + (((v_address >> (12 + 10 * i)) & PN_MASK) * PTE_SIZE);

		if (mem_read_u32(pte_addr, &pte) != MMU_OK) {
			logger("Cannot access pte at 0x%x\n", a);
			return MMU_ACCESS_FAULT;
		}

		logger("pte_addr=0x%x\n", pte_addr);
		logger("pte=0x%x\n", pte);

		if (!(pte & PTE_V_MASK) || (!(pte & PTE_R_MASK) && (pte & PTE_W_MASK))) {
			logger("PTE BROKEN\n");
			return MMU_PAGE_FAULT;
		}

		if ((pte & PTE_R_MASK) || (pte & PTE_X_MASK)) {
			xwr = (pte >> 1) & 7;
			if (xwr == 2 || xwr == 6) {
				logger("RESERVED IN XWR\n");
				return MMU_PAGE_FAULT;
			}

			if (priv == 1 || (s->mstatus_mprv && (s->mstatus_mpp == 0x1))) {
				logger("S check\n");
				if ((pte & PTE_U_MASK) && !s->mstatus_sum) {
					logger("S IN NOT ABLE TO ACCESS U\n");
					return MMU_PAGE_FAULT;
				}
			} else {
				if (!(pte & PTE_U_MASK)) {
					logger("PRV_U trying to access not U\n");
					return MMU_PAGE_FAULT;
				}
			}

			if (s->mstatus_mxr) {
				xwr |= (xwr >> 2);
			}

			if (((xwr >> access) & 1) == 0) {
				logger("access xwr failed\n");
				return MMU_PAGE_FAULT;
			}

			if (i > 0 && ((pte >> 10) & PN_MASK)) {
				logger("superpage misaligned\n");
				return MMU_PAGE_FAULT;
			}

			need_write = !(pte & PTE_A_MASK) ||
					(!(pte & PTE_D_MASK) && access == WRITE);
			pte |= PTE_A_MASK;
			if (access == WRITE) {
				pte |= PTE_D_MASK;
			}
			if (need_write) {
				if (mem_write_u32(pte_addr, pte) != MMU_OK) {
					return MMU_ACCESS_FAULT;
				}
			}

			if (i > 0) {
				logger("superpage found\n");
				p_addr |= ((pte >> 20) << 22) | (((v_address >> 12) & PN_MASK) << 12);
			} else if (i == 0) {
				logger("4kb page\n");
				p_addr |= (pte >> 10) << PG_SHIFT;
			} else {
				return MMU_PAGE_FAULT;
			}

			p_addr |= v_address & 0xfff;
			logger("MMU OK: paddr=0x%x vaddr=0x%x pte=0x%x\n", p_addr, v_address, pte);

			*p_address = (uint32_t)p_addr;

			return MMU_OK;
		} else {
			i = i - 1;
			if (i < 0) {
				return MMU_PAGE_FAULT;
			}
			a = (pte >> 10) << PG_SHIFT;
			continue;
		}
	}
}
