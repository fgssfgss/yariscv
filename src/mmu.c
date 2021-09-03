//
// Created by andrew on 7/27/21.
//

#include "mmu.h"
#include "rv32_p.h"
#include "memmap.h"
#include "tlb.h"

#define PG_SHIFT 12
#define PN_SHIFT 10
#define LEVELS 2
#define PTE_SIZE 4
#define PN_MASK ((1 << PN_SHIFT) - 1)
#define PTE_V_MASK (1 << 0)
#define PTE_R_MASK (1 << 1)
#define PTE_W_MASK (1 << 2)
#define PTE_X_MASK (1 << 3)
#define PTE_U_MASK (1 << 4)
#define PTE_A_MASK (1 << 6)
#define PTE_D_MASK (1 << 7)

// we need it to be able to access satp register and others
static rv32_state *s;

static mmu_error_t translate_address(uint32_t v_address, uint32_t *p_address, access_t access);

static inline mmu_error_t translate_sv32(uint32_t v_address, uint32_t *p_address, access_t access, int priv);

// TEMPLATE MAGIC
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#define CAT(X, Y) X##_##Y
#define TEMPLATE(X, Y) CAT(X,Y)
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
	return res;
}


void mmu_init(rv32_state *state) {
	s = state;
	tlb_init();
	printf("MMU SV32 inited\n");
}

static mmu_error_t translate_address(uint32_t v_address, uint32_t *p_address, access_t access) {
	int priv;
	mmu_error_t ret = MMU_OK;

	if (s->mstatus_mprv && access != EXECUTE) {
		/* use previous privilege */
		priv = s->mstatus_mpp;
	} else {
		priv = s->prv;
	}

	if ((s->satp >> 31) == 1 && (priv != PRV_M)) {
		logger("SV32 triggered, priv is %d, mpp is %d, mprv is %d\n", priv, s->mstatus_mpp, s->mstatus_mprv);
		/* sv32 mode */
		if (tlb_get(v_address, p_address, access)) {
			logger("tlb hit\n");
			return ret;
		} else {
			logger("tlb miss\n");
			ret = translate_sv32(v_address, p_address, access, priv);
			if (ret == MMU_OK) {
				tlb_put(v_address, *p_address, access);
			}
			return ret;
		}
	} else {
		*p_address = v_address;
		return ret;
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

	while (i >= 0) {
		pte_addr = a + (((v_address >> (PG_SHIFT + PN_SHIFT * i)) & PN_MASK) * PTE_SIZE);

		if (mem_read_u32(pte_addr, &pte) != MMU_OK) {
			return MMU_ACCESS_FAULT;
		}

		logger("pte_addr=0x%x\n", pte_addr);
		logger("pte=0x%x\n", pte);

		if (!(pte & PTE_V_MASK) || (!(pte & PTE_R_MASK) && (pte & PTE_W_MASK))) {
			return MMU_PAGE_FAULT;
		}

		if ((pte & PTE_R_MASK) || (pte & PTE_X_MASK)) {
			xwr = (pte >> 1) & 7;
			if (xwr == 2 || xwr == 6) {
				return MMU_PAGE_FAULT;
			}

			if (priv == PRV_S || (s->mstatus_mprv && (s->mstatus_mpp == PRV_S))) {
				if ((pte & PTE_U_MASK) && !s->mstatus_sum) {
					return MMU_PAGE_FAULT;
				}
			} else {
				if (!(pte & PTE_U_MASK)) {
					return MMU_PAGE_FAULT;
				}
			}

			if (s->mstatus_mxr) {
				xwr |= (xwr >> 2);
			}

			if (((xwr >> access) & 1) == 0) {
				return MMU_PAGE_FAULT;
			}

			if (i > 0 && ((pte >> PN_SHIFT) & PN_MASK)) {
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

			p_addr |= v_address & ((1 << PG_SHIFT) - 1);

			if (i > 0) {
				p_addr |= ((pte >> (2 * PN_SHIFT)) << 22) | (((v_address >> PG_SHIFT) & PN_MASK) << PG_SHIFT);
			} else if (i == 0) {
				p_addr |= (pte >> PN_SHIFT) << PG_SHIFT;
			} else {
				return MMU_PAGE_FAULT;
			}

			logger("MMU OK: paddr=0x%x vaddr=0x%x pte=0x%x\n", p_addr, v_address, pte);
			*p_address = (uint32_t) p_addr;

			return MMU_OK;
		} else {
			i = i - 1;
			if (i < 0) {
				return MMU_PAGE_FAULT;
			}
			a = (pte >> PN_SHIFT) << PG_SHIFT;
			continue;
		}
	}
}
