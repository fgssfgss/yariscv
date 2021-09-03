//
// Created by andrew on 7/25/21.
//

#include "rv32.h"
#include "rv32_p.h"
#include "mmu.h"
#include "clint.h"
#include "logger.h"
#include "tlb.h"

#define get_instr_piece(opcode, start, mask) (((opcode) >> start) & mask)
#define GET_OPCODE(opcode) (get_instr_piece(opcode, 0, 0x7f))
#define GET_FUNCT7(opcode) (get_instr_piece(opcode, 25, 0x7f))
#define GET_FUNCT3(opcode) (get_instr_piece(opcode, 12, 0x7))
#define GET_RS2(opcode) (get_instr_piece(opcode, 20, 0x1f))
#define GET_RS1(opcode) (get_instr_piece(opcode, 15, 0x1f))
#define GET_RD(opcode) (get_instr_piece(opcode, 7, 0x1f))
#define GET_IMM_U(opcode) (sext(get_instr_piece(opcode, 12, 0xfffff) << 12, 32))
#define GET_IMM_I(opcode) (sext(get_instr_piece(opcode, 20, 0xfff), 12))
#define GET_IMM_S(opcode) (sext(get_instr_piece(opcode, 7, 0x1f) | (get_instr_piece(opcode, 25, 0x7f) << 5), 12))
#define GET_IMM_B(opcode) (sext((get_instr_piece(opcode, 8, 0xf) << 1) |\
                                 (get_instr_piece(opcode, 25, 0x3f) << 5) |\
                                 (get_instr_piece(opcode, 7, 0x1) << 11) |\
                                 (get_instr_piece(opcode, 31, 0x1) << 12), 13))
#define GET_IMM_J(opcode) (sext((get_instr_piece(opcode, 21, 0x3ff) << 1) |\
                                 (get_instr_piece(opcode, 20, 0x1) << 11) |\
                                 (get_instr_piece(opcode, 12, 0xff) << 12) |\
                                 (get_instr_piece(opcode, 31, 0x1) << 20), 21))


/* MACRO FOR COMPRESSED INSTR */
#define GET_C_FUNCT3(opcode) (get_instr_piece(opcode, 13, 0x7))
#define GET_C_RD(opcode) (get_instr_piece(opcode, 7, 0x1f))
#define GET_C_RS2(opcode) (get_instr_piece(opcode, 2, 0x1f))
#define GET_C_RS1_COMP(opcode) (get_instr_piece(opcode, 7, 0x7) | 8)
#define GET_C_RS2_COMP(opcode) (get_instr_piece(opcode, 2, 0x7) | 8)
#define GET_C_IMM_CJ(opcode) (sext(get_instr_piece(opcode, 2, 0x1) << 5 | \
                                   get_instr_piece(opcode, 3, 0x7) << 1 | \
                                   get_instr_piece(opcode, 6, 0x1) << 7 | \
                                   get_instr_piece(opcode, 7, 0x1) << 6 | \
                                   get_instr_piece(opcode, 8, 0x1) << 10 | \
                                   get_instr_piece(opcode, 9, 0x3) << 8 | \
                                   get_instr_piece(opcode, 11, 0x1) << 4 | \
                                   get_instr_piece(opcode, 12, 0x1) << 11, 12))
#define GET_C_IMM_CI(opcode) (get_instr_piece(opcode, 2, 0x3) << 6 | \
                                   get_instr_piece(opcode, 4, 0x7) << 2 | \
                                   get_instr_piece(opcode, 12, 0x1) << 5)
#define GET_C_IMM_CB(opcode) (sext(get_instr_piece(opcode, 2, 0x1f) | \
                                   get_instr_piece(opcode, 12, 0x1) << 5, 6))
#define GET_C_IMM_CBU(opcode) (get_instr_piece(opcode, 2, 0x1f) | \
                                   get_instr_piece(opcode, 12, 0x1) << 5)
#define GET_C_IMM_CS(opcode) (get_instr_piece(opcode, 5, 0x1) << 6 | \
                                   get_instr_piece(opcode, 6, 0x1) << 2 | \
                                   get_instr_piece(opcode, 10, 0x7) << 3)
#define GET_C_IMM_CSS(opcode) (get_instr_piece(opcode, 7, 0x3) << 6 | \
                                    get_instr_piece(opcode, 9, 0xf) << 2)
#define GET_C_IMM_CSP(opcode) (sext(get_instr_piece(opcode, 2, 0x1) << 5 | \
                                    get_instr_piece(opcode, 3, 0x3) << 7 | \
                                    get_instr_piece(opcode, 5, 0x1) << 6 | \
                                    get_instr_piece(opcode, 6, 0x1) << 4 | \
                                    get_instr_piece(opcode, 12, 0x1) << 9, 10))
#define GET_C_IMM_CIW(opcode) (get_instr_piece(opcode, 5, 0x1) << 3 | \
                                    get_instr_piece(opcode, 6, 0x1) << 2 | \
                                    get_instr_piece(opcode, 7, 0xf) << 6 | \
                                    get_instr_piece(opcode, 11, 0x3) << 4)
#define GET_C_IMM_CBR(opcode) (sext(get_instr_piece(opcode, 2, 0x1) << 5 | \
                                    get_instr_piece(opcode, 3, 0x3) << 1 | \
                                    get_instr_piece(opcode, 5, 0x3) << 6 | \
                                    get_instr_piece(opcode, 10, 0x3) << 3 | \
                                    get_instr_piece(opcode, 12, 0x1) << 8, 9))


#define PR(fmt, ...) do {                         \
            logger(fmt "\n", ##__VA_ARGS__);      \
} while(0)

#define NEXT_INSTR do {                 \
            s.pc += 4;                  \
            goto next_instr;            \
} while(0)

#define C_NEXT_INSTR do {               \
            s.pc += 2;                  \
            goto next_instr;            \
} while(0)

#define JUMP_INSTR do {                 \
            goto next_instr;            \
} while(0)

#define ILL_INSTR do {                  \
            goto illegal_instr;         \
} while(0)

#define MMU_EXCPT(excpt, tval) do {      \
            s.pending_exception = excpt; \
            s.pending_tval = tval;       \
            goto exception;              \
} while(0)

#define AMO_MEM_FAULT MMU_EXCPT((res == MMU_ACCESS_FAULT) ? STORE_ACCESS_FAULT : STORE_PAGE_FAULT, s.reg[rs1])
#define LOAD_MEM_FAULT MMU_EXCPT((res == MMU_ACCESS_FAULT) ? LOAD_ACCESS_FAULT : LOAD_PAGE_FAULT, addr)
#define STORE_MEM_FAULT MMU_EXCPT((res == MMU_ACCESS_FAULT) ? STORE_ACCESS_FAULT : STORE_PAGE_FAULT, addr)

static rv32_state s;

static inline int32_t sext(int32_t val, int n) {
	return (val << (32 - n)) >> (32 - n);
}

static inline int32_t div(int32_t a, int32_t b) {
	if (b == 0) {
		return -1;
	} else if (a == ((int32_t) 1 << 31) && b == -1) {
		return a;
	} else {
		return a / b;
	}
}

static inline int32_t divu(int32_t a, int32_t b) {
	if (b == 0) {
		return -1;
	} else {
		return a / b;
	}
}

static inline int32_t rem(int32_t a, int32_t b) {
	if (b == 0) {
		return a;
	} else if (a == ((int32_t) 1 << 31) && b == -1) {
		return 0;
	} else {
		return a % b;
	}
}

static inline int32_t remu(int32_t a, int32_t b) {
	if (b == 0) {
		return a;
	} else {
		return a % b;
	}
}

static inline uint32_t mulh(int32_t a, int32_t b) {
	return ((int64_t) a * (int64_t) b) >> 32;
}

static inline uint32_t mulhsu(int32_t a, uint32_t b) {
	return ((int64_t) a * (int64_t) b) >> 32;
}

static inline uint32_t mulhu(uint32_t a, uint32_t b) {
	return ((int64_t) a * (int64_t) b) >> 32;
}

static char *abi_iregs[] = {
		"zero",
		"ra",
		"sp",
		"gp",
		"tp",
		"t0",
		"t1",
		"t2",
		"s0",
		"s1",
		"a0",
		"a1",
		"a2",
		"a3",
		"a4",
		"a5",
		"a6",
		"a7",
		"s2",
		"s3",
		"s4",
		"s5",
		"s6",
		"s7",
		"s8",
		"s9",
		"s10",
		"s11",
		"t3",
		"t4",
		"t5",
		"t6"
};

static inline char *get_ireg_name(uint8_t ireg) {
	return abi_iregs[ireg];
}

typedef enum {
	OP = 0b0110011,
	OP_IMM = 0b0010011,
	BRANCH = 0b1100011,
	LUI = 0b0110111,
	AUIPC = 0b0010111,
	JAL = 0b1101111,
	JALR = 0b1100111,
	LOAD = 0b0000011,
	STORE = 0b0100011,
	MISC_MEM = 0b0001111,
	SYSTEM = 0b1110011,
	ATOMICS = 0b0101111
} ops;

#define MSTATUS_SIE_SHIFT 1
#define MSTATUS_MIE_SHIFT 3
#define MSTATUS_SPIE_SHIFT 5
#define MSTATUS_MPIE_SHIFT 7
#define MSTATUS_SPP_SHIFT 8
#define MSTATUS_MPP_SHIFT 11
#define MSTATUS_FS_SHIFT 13
#define MSTATUS_XS_SHIFT 15
#define MSTATUS_MPRV_SHIFT 17
#define MSTATUS_SUM_SHIFT 18
#define MSTATUS_MXR_SHIFT 19
#define MSTATUS_SD_SHIFT 31

typedef enum {
	MSTATUS_SIE = (1 << MSTATUS_SIE_SHIFT),
	MSTATUS_MIE = (1 << MSTATUS_MIE_SHIFT),
	MSTATUS_SPIE = (1 << MSTATUS_SPIE_SHIFT),
	MSTATUS_MPIE = (1 << MSTATUS_MPIE_SHIFT),
	MSTATUS_SPP = (1 << MSTATUS_SPP_SHIFT),
	MSTATUS_MPP = (3 << MSTATUS_MPP_SHIFT),
	MSTATUS_FS = (3 << MSTATUS_FS_SHIFT),
	MSTATUS_XS = (3 << MSTATUS_XS_SHIFT),
	MSTATUS_MPRV = (1 << MSTATUS_MPRV_SHIFT),
	MSTATUS_SUM = (1 << MSTATUS_SUM_SHIFT),
	MSTATUS_MXR = (1 << MSTATUS_MXR_SHIFT),
	MSTATUS_SD = (1 << MSTATUS_SD_SHIFT)
} mstatus_flags;

const uint32_t mstatus_mask = MSTATUS_SIE | MSTATUS_MIE | MSTATUS_SPIE | MSTATUS_MPIE
                              | MSTATUS_SPP | MSTATUS_MPP | MSTATUS_FS | MSTATUS_XS
                              | MSTATUS_MPRV | MSTATUS_SUM | MSTATUS_MXR | MSTATUS_SD;

const uint32_t sstatus_mask = MSTATUS_SIE | MSTATUS_SPIE | MSTATUS_SPP | MSTATUS_FS
                              | MSTATUS_XS | MSTATUS_SUM | MSTATUS_MXR | MSTATUS_SD;

const uint32_t xcounteren_mask = ((1 << 0) | (1 << 2));

typedef enum {
	INSTRUCTION_ADDRESS_MISALIGNED = 0,
	INSTRUCTION_ACCESS_FAULT = 1,
	ILLEGAL_INSTRUCTION = 2,
	BREAKPOINT = 3,
	LOAD_ADDRESS_MISALIGNED = 4,
	LOAD_ACCESS_FAULT = 5,
	STORE_ADDRESS_MISALIGNED = 6,
	STORE_ACCESS_FAULT = 7,
	ENVIRONMENT_CALL_FROM_UMODE = 8,
	ENVIRONMENT_CALL_FROM_SMODE = 9,
	ENVIRONMENT_CALL_FROM_MMODE = 11,
	INSTRUCTION_PAGE_FAULT = 12,
	LOAD_PAGE_FAULT = 13,
	STORE_PAGE_FAULT = 15,

	SUPERVISOR_SOFTWARE_INTERRUPT = 0x80000000 + 1,
	MACHINE_SOFTWARE_INTERRUPT = 0x80000000 + 3,
	SUPERVISOR_TIMER_INTERRUPT = 0x80000000 + 5,
	MACHINE_TIMER_INTERRUPT = 0x80000000 + 7,
	SUPERVISOR_EXTERNAL_INTERRUPT = 0x80000000 + 9,
	MACHINE_EXTERNAL_INTERRUPT = 0x80000000 + 11
} traps;

#define MISA_SUPER   (1 << ('S' - 'A'))
#define MISA_USER    (1 << ('U' - 'A'))
#define MISA_I       (1 << ('I' - 'A'))
#define MISA_M       (1 << ('M' - 'A'))
#define MISA_A       (1 << ('A' - 'A'))
#define MISA_C       (1 << ('C' - 'A'))
#define MISA_32BIT   (1 << 30)

static inline char *get_op_name(ops op) {
#define TYPE2STR(a_type) case a_type: return #a_type;
	switch (op) {
		TYPE2STR(OP)
		TYPE2STR(OP_IMM)
		TYPE2STR(BRANCH)
		TYPE2STR(LUI)
		TYPE2STR(AUIPC)
		TYPE2STR(JAL)
		TYPE2STR(JALR)
		TYPE2STR(LOAD)
		TYPE2STR(STORE)
		TYPE2STR(MISC_MEM)
		TYPE2STR(SYSTEM)
		TYPE2STR(ATOMICS)
		default:
			return "UNKNOWN OPCODE";
	}
#undef TYPE2STR
}

static inline void mstatus_dump(void) {
	logger("sie %d | mie %d | spie %d | mpie %d | spp %d | mpp %d | fs %d | xs %d | sum %d | mxr %d | mprv %d | sd %d\n",
	       s.mstatus_sie, s.mstatus_mie, s.mstatus_spie,
	       s.mstatus_mpie, s.mstatus_spp, s.mstatus_mpp,
	       s.mstatus_fs, s.mstatus_xs, s.mstatus_sum,
	       s.mstatus_mxr, s.mstatus_mprv, s.mstatus_sd);
}

static inline uint32_t mstatus_get(uint32_t mask) {
	uint32_t val = 0;
	s.mstatus_sd = (s.mstatus_fs == MSTATUS_FS) ||
	               (s.mstatus_xs == MSTATUS_XS);

	val = (s.mstatus_sie << MSTATUS_SIE_SHIFT) |
	      (s.mstatus_mie << MSTATUS_MIE_SHIFT) |
	      (s.mstatus_spie << MSTATUS_SPIE_SHIFT) |
	      (s.mstatus_mpie << MSTATUS_MPIE_SHIFT) |
	      (s.mstatus_spp << MSTATUS_SPP_SHIFT) |
	      (s.mstatus_mpp << MSTATUS_MPP_SHIFT) |
	      (s.mstatus_fs << MSTATUS_FS_SHIFT) |
	      (s.mstatus_xs << MSTATUS_XS_SHIFT) |
	      (s.mstatus_sum << MSTATUS_SUM_SHIFT) |
	      (s.mstatus_mxr << MSTATUS_MXR_SHIFT) |
	      (s.mstatus_mprv << MSTATUS_MPRV_SHIFT) |
	      (s.mstatus_sd << MSTATUS_SD_SHIFT);

	val &= mask;
	return val;
}

static inline void mstatus_set(uint32_t val, uint32_t mask) {
	/* maybe do something with fs */

	val = (mstatus_get(-1) & ~mask) | (val & mask);

	s.mstatus_sie = (val >> MSTATUS_SIE_SHIFT) & 0x1;
	s.mstatus_mie = (val >> MSTATUS_MIE_SHIFT) & 0x1;
	s.mstatus_spie = (val >> MSTATUS_SPIE_SHIFT) & 0x1;
	s.mstatus_mpie = (val >> MSTATUS_MPIE_SHIFT) & 0x1;
	s.mstatus_spp = (val >> MSTATUS_SPP_SHIFT) & 0x1;
	s.mstatus_mpp = (val >> MSTATUS_MPP_SHIFT) & 0x3;
	s.mstatus_fs = (val >> MSTATUS_FS_SHIFT) & 0x3;
	s.mstatus_xs = (val >> MSTATUS_XS_SHIFT) & 0x3;
	s.mstatus_sum = (val >> MSTATUS_SUM_SHIFT) & 0x1;
	s.mstatus_mxr = (val >> MSTATUS_MXR_SHIFT) & 0x1;
	s.mstatus_mprv = (val >> MSTATUS_MPRV_SHIFT) & 0x1;
	s.mstatus_sd = (val >> MSTATUS_SD_SHIFT) & 0x1;

	logger("status set to 0x%x\n", val);
	mstatus_dump();
}

static inline void set_prv(uint8_t prv) {
	if (s.prv != prv) {
		s.prv = prv;
	}
}

static void handle_mret() {
	s.mstatus_mie = s.mstatus_mpie;
	s.mstatus_mpie = 1;

	if (s.mstatus_mpp != PRV_M) {
		s.mstatus_mprv = 0;
	}

	set_prv(s.mstatus_mpp);
	s.mstatus_mpp = PRV_U;
	s.pc = s.mepc;

	mstatus_dump();
}

static void handle_sret() {
	s.mstatus_sie = s.mstatus_spie;
	s.mstatus_spie = 1;

	if (s.mstatus_spp != PRV_M) {
		s.mstatus_mprv = 0;
	}

	set_prv(s.mstatus_spp);
	s.mstatus_spp = PRV_U;
	s.pc = s.sepc;

	mstatus_dump();
}

static void raise_exception(uint32_t cause, uint32_t tval) {
	bool s_prv = false;
	bool is_interrupt = cause & 0x80000000;
	uint32_t icause = cause & 31;

	if (s.prv <= PRV_S) {
		if (is_interrupt) {
			s_prv = ((s.mideleg >> icause) & 0x1);
		} else {
			s_prv = ((s.medeleg >> cause) & 0x1);
		}
	} else {
		s_prv = false;
	}

	if (s_prv) {
		s.mstatus_spie = s.mstatus_sie;
		s.mstatus_sie = 0;
		s.mstatus_spp = s.prv;
		set_prv(PRV_S);
		s.scause = cause;
		s.stval = tval;
		s.sepc = s.pc;
		s.pc = s.stvec + (s.s_vectored_interrupts && is_interrupt ? icause * 4 : 0);
	} else {
		s.mstatus_mpie = s.mstatus_mie;
		s.mstatus_mie = 0;
		s.mstatus_mpp = s.prv;
		set_prv(PRV_M);
		s.mcause = cause;
		s.mtval = tval;
		s.mepc = s.pc;
		s.pc = s.mtvec + (s.m_vectored_interrupts && is_interrupt ? icause * 4 : 0);
	}

	logger("raise_exception: %s mode, pc is 0x%x, cause is 0x%x, tval is 0x%x, epc 0x%x, prev mode is %d\n",
	       s_prv ? "PRV_S" : "PRV_M", s.pc, cause, tval, s_prv ? s.sepc : s.mepc, s.prv);
	mstatus_dump();
}

static void raise_interrupt(traps num) {
	raise_exception(num, 0);
}

// true if read successful, false for invalid csr or unable to read
static bool csr_read(uint32_t csr, uint32_t *data) {
	uint32_t val = 0;

	if (s.prv < ((csr >> 8) & 3)) {
		return false;
	}

	switch (csr) {
		case 0xc00: // cycle
		case 0xc02: // instret
			if (s.prv < PRV_M) {
				uint32_t counteren;
				if (s.prv < PRV_S) {
					counteren = s.scounteren;
				} else {
					counteren = s.mcounteren;
				}
				if (((counteren >> (csr & 0x1f)) & 1) == 0) {
					return false;
				}
			}
			val = s.instr_cnt & 0xffffffff;
			break;
		case 0xc80: // cycleh
		case 0xc82: // instreth
			if (s.prv < PRV_M) {
				uint32_t counteren;
				if (s.prv < PRV_S) {
					counteren = s.scounteren;
				} else {
					counteren = s.mcounteren;
				}
				if (((counteren >> (csr & 0x1f)) & 1) == 0) {
					return false;
				}
			}
			val = s.instr_cnt >> 32;
			break;
		case 0xc01: // time
			val = clint_mtime() & 0xffffffff;
			break;
		case 0xc81: // timeh
			val = clint_mtime() >> 32;
			break;
		case 0x100: // sstatus
			val = mstatus_get(sstatus_mask);
			break;
		case 0x104: // sie
			val = s.mie & s.mideleg;
			break;
		case 0x105: // stvec
			val = s.stvec;
			break;
		case 0x106: // scounteren
			val = s.scounteren;
			break;
		case 0x140: // sscratch
			val = s.sscratch;
			break;
		case 0x141: // sepc
			val = s.sepc;
			break;
		case 0x142: // scause
			val = s.scause;
			break;
		case 0x143: // stval
			val = s.stval;
			break;
		case 0x144: // sip
			val = s.mip & s.mideleg;
			break;
		case 0x180: // satp
			val = s.satp;
			logger("satp is 0x%x\n", s.satp);
			break;
		case 0x300: // mstatus
			val = mstatus_get(mstatus_mask);
			break;
		case 0x301: // misa
			val = s.misa;
			break;
		case 0x302: // medeleg
			val = s.medeleg;
			break;
		case 0x303: // mideleg
			val = s.mideleg;
			break;
		case 0x304: // mie
			val = s.mie;
			break;
		case 0x305: // mtvec
			val = s.mtvec;
			break;
		case 0x306: // mcounteren
			val = s.mcounteren;
			break;
		case 0x310: // mstatush (rv32 only)
			val = 0;
			break;
		case 0x340: // mscratch
			val = s.mscratch;
			break;
		case 0x341: // mepc
			val = s.mepc;
			break;
		case 0x342: // mcause
			val = s.mcause;
			break;
		case 0x343: // mtval
			val = s.mtval;
			break;
		case 0x344: // mip
			val = s.mip;
			break;
		case 0xf14: // mhartid
			val = s.mhartid;
			break;
		case 0x3a0:
		case 0x3a1:
		case 0x3a2:
		case 0x3a3:
			val = 0;
			break;
		case 0x3b0:
		case 0x3b1:
		case 0x3b2:
		case 0x3b3:
		case 0x3b4:
		case 0x3b5:
		case 0x3b6:
		case 0x3b7:
		case 0x3b8:
		case 0x3b9:
		case 0x3ba:
		case 0x3bb:
		case 0x3bc:
		case 0x3bd:
		case 0x3be:
		case 0x3bf:
			val = 0;
			break;
		default:
			return false;
	}

	*data = val;

	return true;
}

static bool csr_write(uint32_t csr, uint32_t data) {
	if ((csr & 0xc00) == 0xc00) {
		return false;
	}

	switch (csr) {
		case 0x100: // sstatus
			mstatus_set(data, sstatus_mask);
			break;
		case 0x104: // sie
			s.mie = (s.mie & ~s.mideleg) | (data & s.mideleg);
			break;
		case 0x105: // stvec
			s.s_vectored_interrupts = data & 0x1;
			s.stvec = data & ~3;
			break;
		case 0x106: // scounteren
			s.scounteren = data & xcounteren_mask;
			break;
		case 0x140: // sscratch
			s.sscratch = data;
			break;
		case 0x141: // sepc
			s.sepc = data & ~1;
			break;
		case 0x142: // scause
			s.scause = data;
			break;
		case 0x143: // stval
			s.stval = data;
			break;
		case 0x144: // sip
			s.mip = (s.mip & ~s.mideleg) | (data & s.mideleg);
			break;
		case 0x180: // satp
			/* remove ASID */
			data &= ~(((1 << 9) - 1) << 22);
			s.satp = data;
			logger("satp set to 0x%x\n", s.satp);
			tlb_flush();
			break;
		case 0x300: // mstatus
			mstatus_set(data, mstatus_mask);
			break;
		case 0x301: // misa
			break;
		case 0x302: {
			const uint32_t mask = (1 << (STORE_PAGE_FAULT + 1)) - 1;
			s.medeleg = (s.medeleg & ~mask) | (data & mask);
		}
			break;
		case 0x303: {
			const uint32_t mask = MIP_SSIP | MIP_STIP | MIP_SEIP;
			s.mideleg = (s.mideleg & ~mask) | (data & mask);
		}
			break;
		case 0x304: {
			const uint32_t mask = MIP_MSIP | MIP_MTIP | MIP_MEIP | MIP_SSIP | MIP_STIP | MIP_SEIP;
			s.mie = (s.mie & ~mask) | (data & mask);
		}
			break;
		case 0x305: // mtvec
			s.m_vectored_interrupts = data & 0x1;
			s.mtvec = data & ~3;
			break;
		case 0x306: // mcounteren
			s.mcounteren = data & xcounteren_mask;
			break;
		case 0x310: // mstatush (rv32 only)
			break;
		case 0x340: // mscratch
			s.mscratch = data;
			break;
		case 0x341: // mepc
			s.mepc = data & ~1;
			break;
		case 0x342: // mcause
			s.mcause = data;
			break;
		case 0x343: // mtval
			s.mtval = data;
			break;
		case 0x344: // mip
			s.mip = data;
			break;
		case 0x3a0:
		case 0x3a1:
		case 0x3a2:
		case 0x3a3:
			/* PMPCFGx not implemented */
			break;
		case 0x3b0:
		case 0x3b1:
		case 0x3b2:
		case 0x3b3:
		case 0x3b4:
		case 0x3b5:
		case 0x3b6:
		case 0x3b7:
		case 0x3b8:
		case 0x3b9:
		case 0x3ba:
		case 0x3bb:
		case 0x3bc:
		case 0x3bd:
		case 0x3be:
		case 0x3bf:
			/* PMPADDRx not implemented */
			break;
		default:
			return false;
	}

	return true;
}

static inline bool rv32_exec_compressed_instr(uint16_t instr) {
	uint8_t opcode = instr & 0x3;

	switch (opcode) {
		case 0x0:
		case 0x1:
		case 0x2:
			break;
		default:
			goto not_compressed_instr;
	}

	PR("OPCODE IS 0x%04x", instr);

	switch (opcode) {
		case 0x0: { // quadrant 0
			if (instr == 0x0) {
				ILL_INSTR;
			}

			uint8_t funct3 = GET_C_FUNCT3(instr);

			switch (funct3) {
				case 0b000: { // C.ADDI4SPN
					uint8_t rd = GET_C_RS2_COMP(instr);
					int32_t imm = GET_C_IMM_CIW(instr);

					PR("C.ADDI4SPN %s, 0x%x", get_ireg_name(rd), imm);

					if (imm != 0) {
						s.reg[rd] = s.reg[2] + imm;
					} else {
						ILL_INSTR;
					}

					C_NEXT_INSTR;
				}
				case 0b010: { // C.LW
					uint8_t rs1 = GET_C_RS1_COMP(instr);
					uint8_t rs2 = GET_C_RS2_COMP(instr);
					uint32_t imm = GET_C_IMM_CS(instr);
					uint32_t addr = (int32_t) (s.reg[rs1] + imm);
					uint32_t data = 0;
					mmu_error_t res = MMU_OK;

					PR("C.LW %s %s %x", get_ireg_name(rs1), get_ireg_name(rs2), imm);

					if ((res = mmu_read_u32(addr, &data)) != MMU_OK) {
						LOAD_MEM_FAULT;
					}

					s.reg[rs2] = data;
					C_NEXT_INSTR;
				}
				case 0b110: { // C.SW
					uint8_t rs1 = GET_C_RS1_COMP(instr);
					uint8_t rs2 = GET_C_RS2_COMP(instr);
					uint32_t imm = GET_C_IMM_CS(instr);
					uint32_t addr = (int32_t) (s.reg[rs1] + imm);
					mmu_error_t res = MMU_OK;

					PR("C.SW %s %s %x", get_ireg_name(rs1), get_ireg_name(rs2), imm);

					if ((res = mmu_write_u32(addr, s.reg[rs2])) != MMU_OK) {
						STORE_MEM_FAULT;
					}

					C_NEXT_INSTR;
				}
				default:
					ILL_INSTR;
			}
		}
			break;
		case 0x1: { // quadrant 1
			uint8_t funct3 = GET_C_FUNCT3(instr);

			switch (funct3) {
				case 0b000: { // C.ADDI
					uint8_t rd = GET_C_RD(instr);
					int32_t imm = GET_C_IMM_CB(instr);

					if (rd != 0) {
						PR("C.ADDI %s, %x", get_ireg_name(rd), imm);

						s.reg[rd] = s.reg[rd] + imm;
					} else { // C.NOP
						PR("C.NOP");
					}

					C_NEXT_INSTR;
				}
				case 0b001: { // C.JAL
					int32_t imm = GET_C_IMM_CJ(instr);
					uint32_t val = s.pc + 2;

					PR("C.JAL ra, 0x%x", imm);

					s.pc = (int32_t) (s.pc + imm);
					s.reg[1] = val;

					JUMP_INSTR;
				}
				case 0b010: { // C.LI
					uint8_t rd = GET_C_RD(instr);
					int32_t imm = GET_C_IMM_CB(instr);
					PR("C.LI %s %x", get_ireg_name(rd), imm);
					if (rd != 0) {
						s.reg[rd] = imm;
					}
					C_NEXT_INSTR;
				}
				case 0b011: {
					uint8_t rd = GET_C_RD(instr);

					if (rd == 2) { // C.ADDI16SP
						int32_t imm = GET_C_IMM_CSP(instr);
						if (imm == 0) {
							ILL_INSTR;
						}
						PR("C.ADDI16SP 0x%x", imm);
						s.reg[2] = (int32_t) (s.reg[2] + imm);
						C_NEXT_INSTR;
					} else { // C.LUI
						uint8_t rd = GET_C_RD(instr);
						int32_t imm = sext(GET_C_IMM_CB(instr) << 12, 18);
						PR("C.LUI %s 0x%x", get_ireg_name(rd), imm);
						if (rd != 0 && rd != 2) {
							s.reg[rd] = imm;
						}
						C_NEXT_INSTR;
					}
				}
				case 0b100: {
					uint8_t param1 = get_instr_piece(instr, 10, 0x3);

					if (param1 == 0x0) { // C.SRLI
						uint8_t rd = GET_C_RS1_COMP(instr);
						uint8_t shamt = GET_C_IMM_CBU(instr);

						PR("C.SRLI %s %x", get_ireg_name(rd), shamt);

						if (!(shamt & (1 << 5))) {
							s.reg[rd] = s.reg[rd] >> shamt;
						} else {
							ILL_INSTR;
						}

						C_NEXT_INSTR;
					} else if (param1 == 0x1) { // C.SRAI
						uint8_t rd = GET_C_RS1_COMP(instr);
						uint8_t shamt = GET_C_IMM_CBU(instr);

						PR("C.SRAI %s %x", get_ireg_name(rd), shamt);

						if (!(shamt & (1 << 5))) {
							s.reg[rd] = (int32_t) s.reg[rd] >> shamt;
						}

						C_NEXT_INSTR;
					} else if (param1 == 0x2) { // C.ANDI
						uint8_t rd = GET_C_RS1_COMP(instr);
						int32_t imm = GET_C_IMM_CB(instr);
						PR("C.ANDI %s 0x%x", get_ireg_name(rd), imm);
						s.reg[rd] = s.reg[rd] & imm;
						C_NEXT_INSTR;
					} else {
						uint8_t param2 = get_instr_piece(instr, 5, 0x3)
						                 | get_instr_piece(instr, 12, 0x1) << 3;

						uint8_t rd = GET_C_RS1_COMP(instr);
						uint8_t rs2 = GET_C_RS2_COMP(instr);

						switch (param2) {
							case 0: { // C.SUB
								PR("C.SUB %s %s %s", get_ireg_name(rd), get_ireg_name(rd), get_ireg_name(rs2));
								s.reg[rd] = (int32_t) (s.reg[rd] - s.reg[rs2]);
								C_NEXT_INSTR;
							}
							case 1: { // C.XOR
								PR("C.XOR %s %s %s", get_ireg_name(rd), get_ireg_name(rd), get_ireg_name(rs2));
								s.reg[rd] = s.reg[rd] ^ s.reg[rs2];
								C_NEXT_INSTR;
							}
							case 2: { // C.OR
								PR("C.OR %s %s %s", get_ireg_name(rd), get_ireg_name(rd), get_ireg_name(rs2));
								s.reg[rd] = s.reg[rd] | s.reg[rs2];
								C_NEXT_INSTR;
							}
							case 3: { // C.AND
								PR("C.AND %s %s %s", get_ireg_name(rd), get_ireg_name(rd), get_ireg_name(rs2));
								s.reg[rd] = s.reg[rd] & s.reg[rs2];
								C_NEXT_INSTR;
							}
							default:
								ILL_INSTR;
						}
					}
				}
				case 0b101: { // C.J
					int32_t imm = GET_C_IMM_CJ(instr);

					PR("C.J 0x%x", imm);

					s.pc = (int32_t) (s.pc + imm);

					JUMP_INSTR;
				}
				case 0b110: { // C.BEQZ
					int32_t imm = GET_C_IMM_CBR(instr);
					uint8_t rs1 = GET_C_RS1_COMP(instr);
					PR("C.BEQZ %s %x", get_ireg_name(rs1), imm);
					if (s.reg[rs1] == 0) {
						s.pc += imm;
						JUMP_INSTR;
					}
					C_NEXT_INSTR;
				}
				case 0b111: { // C.BNEZ
					int32_t imm = GET_C_IMM_CBR(instr);
					uint8_t rs1 = GET_C_RS1_COMP(instr);
					PR("C.BNEZ %s %x", get_ireg_name(rs1), imm);
					if (s.reg[rs1] != 0) {
						s.pc += imm;
						JUMP_INSTR;
					}
					C_NEXT_INSTR;
				}
				default:
					ILL_INSTR;
			}
		}
			break;
		case 0x2: {// quadrant 2
			uint8_t funct3 = GET_C_FUNCT3(instr);

			switch (funct3) {
				case 0b000: { // C.SLLI
					uint8_t rd = GET_C_RD(instr);
					uint8_t shamt = GET_C_IMM_CB(instr);

					PR("C.SLLI %s %x", get_ireg_name(rd), shamt);

					if (shamt & (1 << 5)) {
						ILL_INSTR;
					}

					if (rd != 0) {
						s.reg[rd] = (int32_t) (s.reg[rd] << shamt);
					}

					C_NEXT_INSTR;
				}
				case 0b010: { // C.LWSP
					int32_t imm = GET_C_IMM_CI(instr);
					uint8_t rd = GET_C_RD(instr);
					uint32_t data = 0;
					uint32_t addr = s.reg[2] + imm;
					mmu_error_t res = MMU_OK;

					PR("C.LWSP %s, 0x%x", get_ireg_name(rd), imm);

					if ((res = mmu_read_u32(addr, &data)) != MMU_OK) {
						LOAD_MEM_FAULT;
					}

					if (rd != 0) {
						s.reg[rd] = (int32_t) data;
					}

					C_NEXT_INSTR;
				}
				case 0b100: {
					uint8_t rd = GET_C_RD(instr);
					uint8_t rs2 = GET_C_RS2(instr);

					if (get_instr_piece(instr, 12, 0x1) == 0) {
						if (rs2 == 0) { // C.JR
							if (rd == 0) {
								ILL_INSTR;
							}
							PR("C.JR %s", get_ireg_name(rd));
							s.pc = s.reg[rd] & ~1;
							JUMP_INSTR;
						} else { // C.MV
							PR("C.MV %s, %s", get_ireg_name(rd), get_ireg_name(rs2));
							if (rd != 0) {
								s.reg[rd] = s.reg[rs2];
							}
							C_NEXT_INSTR;
						}
					} else {
						if (rd == 0 && rs2 == 0) { // C.EBREAK
							PR("C.EBREAK");
							s.pending_exception = BREAKPOINT;
							s.pending_tval = 0;
							goto exception;
						} else if (rd != 0 && rs2 == 0) { // C.JALR
							PR("C.JALR %s", get_ireg_name(rd));
							uint32_t val = s.pc + 2;
							s.pc = s.reg[rd] & ~1;
							s.reg[1] = val;
							JUMP_INSTR;
						} else { // C.ADD
							PR("C.ADD %s, %s", get_ireg_name(rd), get_ireg_name(rs2));
							if (rd != 0) {
								s.reg[rd] = (int32_t) (s.reg[rd] + s.reg[rs2]);
							}
							C_NEXT_INSTR;
						}
					}
				}
				case 0b110: { // C.SWSP
					int32_t imm = GET_C_IMM_CSS(instr);
					uint8_t rd = GET_C_RS2(instr);
					uint32_t addr = s.reg[2] + imm;
					mmu_error_t res = MMU_OK;

					PR("C.SWSP %s, 0x%x", get_ireg_name(rd), imm);

					if ((res = mmu_write_u32(addr, s.reg[rd])) != MMU_OK) {
						STORE_MEM_FAULT;
					}

					C_NEXT_INSTR;
				}
				default:
					ILL_INSTR;
			}

			ILL_INSTR;
		}
	}

illegal_instr:
	s.pending_exception = ILLEGAL_INSTRUCTION;
	s.pending_tval = instr;
exception:
	raise_exception(s.pending_exception, s.pending_tval);
next_instr:
	return true;
not_compressed_instr:
	return false;
}

/* This function runs only one opcode and exits */
static inline bool rv32_exec_instr(uint32_t instr) {
	uint8_t opcode = GET_OPCODE(instr);

	if (rv32_exec_compressed_instr(instr & 0xffff)) {
		goto next_instr;
	}

	PR("OPCODE IS 0x%08x", instr);

	switch (opcode) {
		case OP: {
			uint8_t funct3 = GET_FUNCT3(instr);
			uint8_t funct7 = GET_FUNCT7(instr);
			uint8_t rs1 = GET_RS1(instr);
			uint8_t rs2 = GET_RS2(instr);
			uint8_t rd = GET_RD(instr);

			if (funct7 == 0b0000001) {
				switch (funct3) {
					case 0: {
						if (rd != 0) {
							s.reg[rd] = (int32_t) ((int32_t) s.reg[rs1] * (int32_t) s.reg[rs2]);
						}
						PR("MUL %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
						NEXT_INSTR;
					}
					case 1: {
						if (rd != 0) {
							s.reg[rd] = mulh((int32_t) s.reg[rs1], (int32_t) s.reg[rs2]);
						}
						PR("MULH %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
						NEXT_INSTR;
					}
					case 2: {
						if (rd != 0) {
							s.reg[rd] = mulhsu(s.reg[rs1], s.reg[rs2]);
						}
						PR("MULHSU %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
						NEXT_INSTR;
					}
					case 3: {
						if (rd != 0) {
							s.reg[rd] = mulhu(s.reg[rs1], s.reg[rs2]);
						}
						PR("MULHU %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
						NEXT_INSTR;
					}
					case 4: {
						if (rd != 0) {
							s.reg[rd] = div(s.reg[rs1], s.reg[rs2]);
						}
						PR("DIV %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
						NEXT_INSTR;
					}
					case 5: {
						if (rd != 0) {
							s.reg[rd] = divu(s.reg[rs1], s.reg[rs2]);
						}
						PR("DIVU %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
						NEXT_INSTR;
					}
					case 6: {
						if (rd != 0) {
							s.reg[rd] = rem(s.reg[rs1], s.reg[rs2]);
						}
						PR("REM %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
						NEXT_INSTR;
					}
					case 7: {
						if (rd != 0) {
							s.reg[rd] = remu(s.reg[rs1], s.reg[rs2]);
						}
						PR("REMU %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
						NEXT_INSTR;
					}
					default:
						ILL_INSTR;
				}
			} else {
				switch (funct3) {
					case 0: {
						if (funct7 == 0b0100000) {
							if (rd != 0) {
								s.reg[rd] = (int32_t) ((int32_t) s.reg[rs1] - (int32_t) s.reg[rs2]);
							}
							PR("SUB %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
							NEXT_INSTR;
						} else if (funct7 == 0b0000000) {
							if (rd != 0) {
								s.reg[rd] = (int32_t) ((int32_t) s.reg[rs1] + (int32_t) s.reg[rs2]);
							}
							PR("ADD %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
							NEXT_INSTR;
						} else {
							ILL_INSTR;
						}
					}
					case 1: {
						if (rd != 0) {
							s.reg[rd] = (int32_t) (s.reg[rs1] << (s.reg[rs2] & 0x1f));
						}
						PR("SLL %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
						NEXT_INSTR;
					}
					case 2: {
						if (rd != 0) {
							s.reg[rd] = ((int32_t) s.reg[rs1] < (int32_t) s.reg[rs2]) ? 1 : 0;
						}
						PR("SLT %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
						NEXT_INSTR;
					}
					case 3: {
						if (rd != 0) {
							s.reg[rd] = (s.reg[rs1] < s.reg[rs2]) ? 1 : 0;
						}
						PR("SLTU %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
						NEXT_INSTR;
					}
					case 4: {
						if (rd != 0) {
							s.reg[rd] = s.reg[rs1] ^ s.reg[rs2];
						}
						PR("XOR %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
						NEXT_INSTR;
					}
					case 5: {
						if (funct7 == 0b0100000) {
							if (rd != 0) {
								s.reg[rd] = ((int32_t) s.reg[rs1] >> (s.reg[rs2] & 0x1f));
							}

							PR("SRA %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));

							NEXT_INSTR;
						} else {
							if (rd != 0) {
								s.reg[rd] = ((uint32_t) s.reg[rs1] >> (s.reg[rs2] & 0x1f));
							}

							PR("SRL %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));

							NEXT_INSTR;
						}
					}
					case 6: {
						if (rd != 0) {
							s.reg[rd] = s.reg[rs1] | s.reg[rs2];
						}
						PR("OR %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
						NEXT_INSTR;
					}
					case 7: {
						if (rd != 0) {
							s.reg[rd] = (s.reg[rs1] & s.reg[rs2]);
						}
						PR("AND %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
						NEXT_INSTR;
					}
					default:
						ILL_INSTR;
				}
			}
		}
		case OP_IMM: {
			uint8_t funct3 = GET_FUNCT3(instr);
			uint8_t rs1 = GET_RS1(instr);
			uint8_t rd = GET_RD(instr);

			switch (funct3) {
				case 0: {
					int32_t imm = GET_IMM_I(instr);

					if (rd != 0) {
						s.reg[rd] = (int32_t) (s.reg[rs1] + imm);
					}

					PR("ADDI %s %s %d", get_ireg_name(rd), get_ireg_name(rs1), imm);
					NEXT_INSTR;
				}
				case 1: {
					uint8_t shamt = GET_RS2(instr);

					if (rd != 0) {
						s.reg[rd] = (s.reg[rs1] << (shamt & 0x1f));
					}

					PR("SLLI %s %s 0x%x", get_ireg_name(rd), get_ireg_name(rs1), shamt);
					NEXT_INSTR;
				}
				case 2: {
					int32_t imm = GET_IMM_I(instr);

					if (rd != 0) {
						s.reg[rd] = (int32_t) s.reg[rs1] < imm;
					}

					PR("SLTI %s %s %d", get_ireg_name(rd), get_ireg_name(rs1), imm);
					NEXT_INSTR;
				}
				case 3: {
					uint32_t imm = GET_IMM_I(instr);

					if (rd != 0) {
						s.reg[rd] = s.reg[rs1] < imm;
					}

					PR("SLTIU %s %s %d", get_ireg_name(rd), get_ireg_name(rs1), imm);
					NEXT_INSTR;
				}
				case 4: {
					uint32_t imm = GET_IMM_I(instr);

					if (rd != 0) {
						s.reg[rd] = s.reg[rs1] ^ imm;
					}

					PR("XORI %s %s %d", get_ireg_name(rd), get_ireg_name(rs1), imm);
					NEXT_INSTR;
				}
				case 5: {
					uint8_t funct7 = GET_FUNCT7(instr);
					uint8_t shamt = GET_RS2(instr);

					if (funct7 == 0b0100000) {
						if (rd != 0) {
							s.reg[rd] = (int32_t) s.reg[rs1] >> (shamt & 0x1f);
						}

						PR("SRAI %s %s 0x%x", get_ireg_name(rd), get_ireg_name(rs1), shamt);

						NEXT_INSTR;
					} else {
						if (rd != 0) {
							s.reg[rd] = s.reg[rs1] >> (shamt & 0x1f);
						}

						PR("SRLI %s %s 0x%x", get_ireg_name(rd), get_ireg_name(rs1), shamt);

						NEXT_INSTR;
					}
				}
				case 6: {
					uint32_t imm = GET_IMM_I(instr);

					if (rd != 0) {
						s.reg[rd] = s.reg[rs1] | imm;
					}

					PR("ORI %s %s %d", get_ireg_name(rd), get_ireg_name(rs1), imm);
					NEXT_INSTR;
				}
				case 7: {
					uint32_t imm = GET_IMM_I(instr);

					if (rd != 0) {
						s.reg[rd] = s.reg[rs1] & imm;
					}

					PR("ANDI %s %s %d", get_ireg_name(rd), get_ireg_name(rs1), imm);
					NEXT_INSTR;
				}
				default:
					ILL_INSTR;
			}
		}
		case BRANCH: {
			uint8_t rs1 = GET_RS1(instr);
			uint8_t rs2 = GET_RS2(instr);
			uint8_t funct3 = GET_FUNCT3(instr);
			int32_t imm = GET_IMM_B(instr);

			switch (funct3) {
				case 0: {
					PR("BEQ %s %s %d", get_ireg_name(rs1), get_ireg_name(rs2), imm);
					if (s.reg[rs1] == s.reg[rs2]) {
						s.pc += imm;
						JUMP_INSTR;
					} else {
						NEXT_INSTR;
					}
				}
				case 1: {
					PR("BNE %s %s %d", get_ireg_name(rs1), get_ireg_name(rs2), imm);
					if (s.reg[rs1] != s.reg[rs2]) {
						s.pc += imm;
						JUMP_INSTR;
					} else {
						NEXT_INSTR;
					}
				}

				case 4: {
					PR("BLT %s %s %d", get_ireg_name(rs1), get_ireg_name(rs2), imm);
					if ((int32_t) s.reg[rs1] < (int32_t) s.reg[rs2]) {
						s.pc += imm;
						JUMP_INSTR;
					} else {
						NEXT_INSTR;
					}
				}

				case 5: {
					PR("BGE %s %s %d", get_ireg_name(rs1), get_ireg_name(rs2), imm);
					if ((int32_t) s.reg[rs1] >= (int32_t) s.reg[rs2]) {
						s.pc += imm;
						JUMP_INSTR;
					} else {
						NEXT_INSTR;
					}
				}

				case 6: {
					PR("BLTU %s %s %d", get_ireg_name(rs1), get_ireg_name(rs2), imm);
					if (s.reg[rs1] < s.reg[rs2]) {
						s.pc += imm;
						JUMP_INSTR;
					} else {
						NEXT_INSTR;
					}
				}

				case 7: {
					PR("BGEU %s %s %d", get_ireg_name(rs1), get_ireg_name(rs2), imm);
					if (s.reg[rs1] >= s.reg[rs2]) {
						s.pc += imm;
						JUMP_INSTR;
					} else {
						NEXT_INSTR;
					}
				}

				default:
					ILL_INSTR;
			}
		}
		case LUI:
		case AUIPC: {
			int32_t imm = GET_IMM_U(instr);
			uint8_t rd = GET_RD(instr);
			int32_t val = 0;

			PR("%s %s %d", (opcode == LUI) ? "LUI" : "AUIPC", get_ireg_name(rd), imm >> 12);

			if (opcode == AUIPC) {
				imm += s.pc;
			}

			if (rd != 0) {
				s.reg[rd] = imm;
			}

			NEXT_INSTR;
		}
		case JAL: {
			uint8_t rd = GET_RD(instr);
			int32_t imm = GET_IMM_J(instr);
			uint32_t val = s.pc + 4;

			PR("JAL %s 0x%x", get_ireg_name(rd), imm);

			s.pc = (int32_t) (s.pc + imm);

			if (rd != 0) {
				s.reg[rd] = val;
			}

			JUMP_INSTR;
		}
		case JALR: {
			int32_t imm = GET_IMM_I(instr);
			uint8_t funct3 = GET_FUNCT3(instr);
			uint8_t rs1 = GET_RS1(instr);
			uint8_t rd = GET_RD(instr);
			uint32_t val = s.pc + 4;

			PR("JALR %s %s 0x%x", get_ireg_name(rd), get_ireg_name(rs1), imm);

			s.pc = (int32_t) (s.reg[rs1] + imm) & ~1;

			if (rd != 0) {
				s.reg[rd] = val;
			}

			JUMP_INSTR;
		}
		case LOAD: {
			mmu_error_t res = MMU_OK;
			uint8_t rs1 = GET_RS1(instr);
			uint8_t rd = GET_RD(instr);
			uint8_t width = GET_FUNCT3(instr);
			int32_t imm = GET_IMM_I(instr);

			int32_t val = 0;
			uint32_t addr = s.reg[rs1] + imm;

			const char *l_instr[] = {"LB", "LH", "LW", "RESERVED", "LBU", "LHU"};

			PR("%s %s %d(%s)", l_instr[width], get_ireg_name(rd), imm, get_ireg_name(rs1));

			switch (width) {
				case 0: {
					uint8_t data;
					if ((res = mmu_read_u8(addr, &data)) != MMU_OK) {
						LOAD_MEM_FAULT;
					}
					val = sext(data, 8);
					break;
				}
				case 1: {
					uint16_t data;
					if ((res = mmu_read_u16(addr, &data)) != MMU_OK) {
						LOAD_MEM_FAULT;
					}
					val = sext(data, 16);
					break;
				}
				case 2: {
					uint32_t data;
					if ((res = mmu_read_u32(addr, &data)) != MMU_OK) {
						LOAD_MEM_FAULT;
					}
					val = sext(data, 32);
					break;
				}
				case 4: {
					uint8_t data;
					if ((res = mmu_read_u8(addr, &data)) != MMU_OK) {
						LOAD_MEM_FAULT;
					}
					val = data;
					break;
				}
				case 5: {
					uint16_t data;
					if ((res = mmu_read_u16(addr, &data)) != MMU_OK) {
						LOAD_MEM_FAULT;
					}
					val = data;
					break;
				}
				default:
					ILL_INSTR;
			}

			if (rd != 0) {
				s.reg[rd] = val;
			}

			NEXT_INSTR;
		}
		case STORE: {
			mmu_error_t res = MMU_OK;
			uint8_t rs1 = GET_RS1(instr);
			uint8_t rs2 = GET_RS2(instr);
			uint8_t width = GET_FUNCT3(instr);
			int32_t imm = GET_IMM_S(instr);

			uint32_t addr = s.reg[rs1] + imm;
			int32_t val = s.reg[rs2];

			const char *s_instr[] = {"SB", "SH", "SW"};

			switch (width) {
				case 0:
					if ((res = mmu_write_u8(addr, (int8_t) val)) != MMU_OK) {
						STORE_MEM_FAULT;
					}
					break;
				case 1:
					if ((res = mmu_write_u16(addr, (int16_t) val)) != MMU_OK) {
						STORE_MEM_FAULT;
					}
					break;
				case 2:
					if ((res = mmu_write_u32(addr, val)) != MMU_OK) {
						STORE_MEM_FAULT;
					}
					break;
				default:
					ILL_INSTR;
			}

			PR("%s %s %d(%s)", s_instr[width], get_ireg_name(rs2), imm, get_ireg_name(rs1));

			NEXT_INSTR;
		}
		case MISC_MEM: {
			uint8_t funct3 = GET_FUNCT3(instr);

			PR("FENCE");

			switch (funct3) {
				case 0:
					if (instr & 0xf00fff80) {
						ILL_INSTR;
					}
					break;
				case 1:
					if (instr != 0x0000100f) {
						ILL_INSTR;
					}
					break;
				default:
					ILL_INSTR;
			}
			NEXT_INSTR;
		}
		case SYSTEM: {
			uint16_t imm = GET_IMM_I(instr) & 0xfff;
			uint8_t funct3 = GET_FUNCT3(instr);
			uint8_t rs1 = GET_RS1(instr);
			uint8_t rd = GET_RD(instr);

			switch (funct3) {
				case 0: {
					switch (imm) {
						case 0x0:
							if (instr & 0x000fff80) {
								ILL_INSTR;
							}
							PR("ECALL");

							// TODO: add a testmode to catch ECALL with a17 == 93 or a0 with 1

							s.pending_exception = ENVIRONMENT_CALL_FROM_UMODE + s.prv;
							s.pending_tval = 0;
							goto exception;
							break;
						case 0x1:
							if (instr & 0x000fff80) {
								ILL_INSTR;
							}
							PR("EBREAK");
							s.pending_exception = BREAKPOINT;
							s.pending_tval = 0;
							goto exception;
							break;
						case 0x102:
							if (instr & 0x000fff80) {
								ILL_INSTR;
							}
							if (s.prv < PRV_S) {
								ILL_INSTR;
							}
							PR("SRET");
							handle_sret();
							JUMP_INSTR;
							break;
						case 0x105:
							if (instr & 0x00007f80) {
								ILL_INSTR;
							}
							if (s.prv == PRV_U) {
								ILL_INSTR;
							}
							PR("WFI");
							NEXT_INSTR;
							break;
						case 0x302:
							if (instr & 0x000fff80) {
								ILL_INSTR;
							}
							if (s.prv < PRV_M) {
								ILL_INSTR;
							}
							PR("MRET");
							handle_mret();
							JUMP_INSTR;
							break;
						default:
							if ((imm >> 5) == 0b0001001) {
								PR("SFENCE.VMA");
								tlb_flush();
								NEXT_INSTR;
							} else {
								ILL_INSTR;
							}
					}
				}
				case 1: {
					PR("CSRRW %s %s 0x%x", get_ireg_name(rd), get_ireg_name(rs1), imm);

					uint32_t val = 0;
					if (!csr_read(imm, &val)) {
						PR("FAIL TO READ CSR");
						ILL_INSTR;
					}
					if (!csr_write(imm, s.reg[rs1])) {
						PR("FAIL TO WRITE CSR");
						ILL_INSTR;
					}
					if (rd != 0) {
						s.reg[rd] = val;
					}

					NEXT_INSTR;
				}
				case 2: {
					PR("CSRRS %s %s 0x%x", get_ireg_name(rd), get_ireg_name(rs1), imm);

					uint32_t val = 0;
					if (!csr_read(imm, &val)) {
						ILL_INSTR;
					}
					if (s.reg[rs1] != 0) {
						if (!csr_write(imm, s.reg[rs1] | val)) {
							ILL_INSTR;
						}
					}
					if (rd != 0) {
						s.reg[rd] = val;
					}

					NEXT_INSTR;
				}
				case 3: {
					PR("CSRRC %s %s 0x%x", get_ireg_name(rd), get_ireg_name(rs1), imm);

					uint32_t val = 0;
					if (!csr_read(imm, &val)) {
						ILL_INSTR;
					}
					if (s.reg[rs1] != 0) {
						if (!csr_write(imm, val & ~s.reg[rs1])) {
							ILL_INSTR;
						}
					}
					if (rd != 0) {
						s.reg[rd] = val;
					}

					NEXT_INSTR;
				}
				case 5: {
					PR("CSRRWI %s 0x%x 0x%x", get_ireg_name(rd), rs1, imm);

					uint32_t val = 0;
					if (!csr_read(imm, &val)) {
						ILL_INSTR;
					}
					if (!csr_write(imm, rs1)) {
						ILL_INSTR;
					}
					if (rd != 0) {
						s.reg[rd] = val;
					}

					NEXT_INSTR;
				}
				case 6: {
					PR("CSRRSI %s 0x%x 0x%x", get_ireg_name(rd), rs1, imm);

					uint32_t val = 0;
					if (!csr_read(imm, &val)) {
						ILL_INSTR;
					}
					if (rs1 != 0) {
						if (!csr_write(imm, val | rs1)) {
							ILL_INSTR;
						}
					}
					if (rd != 0) {
						s.reg[rd] = val;
					}

					NEXT_INSTR;
				}
				case 7: {
					PR("CSRRCI %s 0x%x 0x%x", get_ireg_name(rd), rs1, imm);

					uint32_t val = 0;
					if (!csr_read(imm, &val)) {
						ILL_INSTR;
					}
					if (rs1 != 0) {
						if (!csr_write(imm, val & ~rs1)) {
							ILL_INSTR;
						}
					}
					if (rd != 0) {
						s.reg[rd] = val;
					}

					NEXT_INSTR;
				}
			}
		}
		case ATOMICS: {
			uint8_t funct3 = GET_FUNCT3(instr);
			uint8_t rd = GET_RD(instr);
			uint8_t rs1 = GET_RS1(instr);
			uint8_t rs2 = GET_RS2(instr);
			uint8_t funct7 = GET_FUNCT7(instr) >> 2; /* remove aq & rl */
			uint32_t val = 0;
			uint32_t val2 = 0;
			mmu_error_t res = MMU_OK;

			if (funct3 != 2) {
				ILL_INSTR;
			}

			switch (funct7) {
				case 2:
					if (rs2 != 0) {
						ILL_INSTR;
					}

					PR("LR %s %s", get_ireg_name(rd), get_ireg_name(rs1));

					if ((res = mmu_read_u32(s.reg[rs1], &val)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					s.lr_addr = s.reg[rs1];

					if (rd != 0) {
						s.reg[rd] = val;
					}
					break;
				case 3:
					PR("SC %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));

					if (s.lr_addr == s.reg[rs1]) {
						if ((res = mmu_write_u32(s.reg[rs1], s.reg[rs2])) != MMU_OK) {
							AMO_MEM_FAULT;
						}

						val = 0;
						s.lr_addr = 0;
					} else {
						val = 1;
					}

					if (rd != 0) {
						s.reg[rd] = val;
					}
					break;
				case 0x0:
					PR("AMOADD %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));
					if ((res = mmu_read_u32(s.reg[rs1], &val)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					val2 = (int32_t) ((int32_t) s.reg[rs2] + (int32_t) val);

					if ((res = mmu_write_u32(s.reg[rs1], val2)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					if (rd != 0) {
						s.reg[rd] = val;
					}
					break;
				case 0x1:
					PR("AMOSWAP %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));

					if ((res = mmu_read_u32(s.reg[rs1], &val)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					val2 = s.reg[rs2];

					if ((res = mmu_write_u32(s.reg[rs1], val2)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					if (rd != 0) {
						s.reg[rd] = val;
					}

					break;
				case 0x4:
					PR("AMOXOR %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));

					if ((res = mmu_read_u32(s.reg[rs1], &val)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					val2 = (int32_t) (s.reg[rs2] ^ val);

					if (mmu_write_u32(s.reg[rs1], val2)) {
						AMO_MEM_FAULT;
					}

					if (rd != 0) {
						s.reg[rd] = val;
					}

					break;
				case 0x8:
					PR("AMOOR %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));

					if ((res = mmu_read_u32(s.reg[rs1], &val)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					val2 = (int32_t) (s.reg[rs2] | val);

					if ((res = mmu_write_u32(s.reg[rs1], val2)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					if (rd != 0) {
						s.reg[rd] = val;
					}

					break;
				case 0xc:
					PR("AMOAND %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));

					if ((res = mmu_read_u32(s.reg[rs1], &val)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					val2 = (int32_t) (s.reg[rs2] & val);

					if ((res = mmu_write_u32(s.reg[rs1], val2)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					if (rd != 0) {
						s.reg[rd] = val;
					}

					break;
				case 0x10:
					PR("AMOMIN %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));

					if ((res = mmu_read_u32(s.reg[rs1], &val)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					if ((int32_t) val < (int32_t) s.reg[rs2]) {
						val2 = (int32_t) val;
					} else {
						val2 = (int32_t) s.reg[rs2];
					}

					if ((res = mmu_write_u32(s.reg[rs1], val2)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					if (rd != 0) {
						s.reg[rd] = val;
					}

					break;
				case 0x14:
					PR("AMOMAX %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));

					if ((res = mmu_read_u32(s.reg[rs1], &val)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					if ((int32_t) val > (int32_t) s.reg[rs2]) {
						val2 = (int32_t) val;
					} else {
						val2 = (int32_t) s.reg[rs2];
					}

					if ((res = mmu_write_u32(s.reg[rs1], val2)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					if (rd != 0) {
						s.reg[rd] = val;
					}

					break;
				case 0x18:
					PR("AMOMINU %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));

					if ((res = mmu_read_u32(s.reg[rs1], &val)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					if (val < s.reg[rs2]) {
						val2 = val;
					} else {
						val2 = (int32_t) s.reg[rs2];
					}

					if ((res = mmu_write_u32(s.reg[rs1], val2)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					if (rd != 0) {
						s.reg[rd] = val;
					}

					break;
				case 0x1c:
					PR("AMOMAXU %s %s %s", get_ireg_name(rd), get_ireg_name(rs1), get_ireg_name(rs2));

					if ((res = mmu_read_u32(s.reg[rs1], &val)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					if (val > s.reg[rs2]) {
						val2 = val;
					} else {
						val2 = (int32_t) s.reg[rs2];
					}

					if ((res = mmu_write_u32(s.reg[rs1], val2)) != MMU_OK) {
						AMO_MEM_FAULT;
					}

					if (rd != 0) {
						s.reg[rd] = val;
					}

					break;
				default:
					ILL_INSTR;
					break;
			}
			NEXT_INSTR;
		}
		default:
			PR("UNKNOWN INSTR");
			ILL_INSTR;
	}

next_instr:
	return false;
illegal_instr:
	s.pending_exception = ILLEGAL_INSTRUCTION;
	s.pending_tval = instr;
exception:
	raise_exception(s.pending_exception, s.pending_tval);
	return true;
}


static inline void dump_cpu_info(void) {
	for (int i = 1; i < 32; ++i) {
		logger("%s = 0x%08x ; ", get_ireg_name(i), s.reg[i]);

		if ((i % 8) == 7) {
			logger("\n");
		}
	}
	logger("\n");
}

bool rv32_get_mip(mip_flags fl) {
	return !!(s.mip & fl);
}

void rv32_reset_mip(mip_flags fl) {
	s.mip &= ~fl;
}

void rv32_set_mip(mip_flags fl) {
	s.mip |= fl;
}

void rv32_raise_interrupts(void) {
	uint32_t pending_ints = s.mie & s.mip;
	uint32_t en_ints = 0;

	if (!pending_ints) {
		return;
	}

	switch (s.prv) {
		case PRV_M:
			if (s.mstatus_mie) {
				en_ints = ~s.mideleg;
			}
			break;
		case PRV_S:
			en_ints = ~s.mideleg;
			if (s.mstatus_sie) {
				en_ints |= s.mideleg;
			}
			break;
		case PRV_U:
			en_ints = -1;
			break;
		default:
			break;
	}

	en_ints &= pending_ints;

	if (en_ints == 0) {
		return;
	}

	if (en_ints & MIP_SSIP) {
		raise_interrupt(SUPERVISOR_SOFTWARE_INTERRUPT);
	} else if (en_ints & MIP_STIP) {
		raise_interrupt(SUPERVISOR_TIMER_INTERRUPT);
	} else if (en_ints & MIP_SEIP) {
		raise_interrupt(SUPERVISOR_EXTERNAL_INTERRUPT);
	} else if (en_ints & MIP_MSIP) {
		raise_interrupt(MACHINE_SOFTWARE_INTERRUPT);
	} else if (en_ints & MIP_MTIP) {
		raise_interrupt(MACHINE_TIMER_INTERRUPT);
	} else if (en_ints & MIP_MEIP) {
		raise_interrupt(MACHINE_EXTERNAL_INTERRUPT);
	}
}

void rv32_run(uint64_t cycles) {
	uint32_t opcode = 0;
	mmu_error_t res = MMU_OK;

	while (cycles--) {
		PR("PC IS 0x%08x", s.pc);
		if ((res = mmu_exec_u32(s.pc, &opcode)) != MMU_OK) {
			raise_exception((res == MMU_ACCESS_FAULT) ? INSTRUCTION_ACCESS_FAULT : INSTRUCTION_PAGE_FAULT, s.pc);
			continue;
		}
		s.instr_cnt++;
		if (rv32_exec_instr(opcode)) {
			continue;
		}
		dump_cpu_info();
		PR("");
	}
}

uint64_t rv32_get_instr_cnt() {
	return s.instr_cnt;
}

void rv32_init(void) {
	s.mhartid = 0;
	s.pc = 0x80000000;
	s.reg[0xa] = 0x0;
	s.reg[0xb] = 0x00001000;

	s.prv = PRV_M;
	s.misa = MISA_SUPER | MISA_USER | MISA_I | MISA_M | MISA_A | MISA_C | MISA_32BIT;

	mmu_init(&s);
}
