//
// Created by andrew on 7/29/21.
//

#ifndef YARISCVEMU_RV32_P_H
#define YARISCVEMU_RV32_P_H

#include <stdbool.h>

#define IREG_MAX 32

typedef enum {
	PRV_U = 0,
	PRV_S = 1,
	PRV_M = 3
} priv_m;

struct rv32_state_s {
    uint32_t reg[IREG_MAX];
    uint32_t pc;

    uint8_t mhartid;
    uint32_t misa;
    uint32_t medeleg;
    uint32_t mideleg;
    uint32_t mie;
    uint32_t mtvec;
    uint32_t mcounteren;
    uint32_t mscratch;
    uint32_t mepc;
    uint32_t mcause;
    uint32_t mtval;
    uint32_t mip;

    uint32_t stvec;
    uint32_t scounteren;
    uint32_t sscratch;
    uint32_t sepc;
    uint32_t scause;
    uint32_t stval;

    uint32_t satp;

    uint8_t prv;

    uint32_t lr_addr;

    bool m_vectored_interrupts;
    bool s_vectored_interrupts;
    int pending_exception;
    uint32_t pending_tval;

    /* mstatus fields, should be composed in mstatus/mstatush*/

    int mstatus_mie;
    int mstatus_sie;

    int mstatus_spie;
    int mstatus_mpie;

    int mstatus_spp;
    int mstatus_mpp;

    int mstatus_fs;
    int mstatus_xs;

    int mstatus_mprv;
    int mstatus_sum;
    int mstatus_mxr;

    int mstatus_sd;
    /* mstatus end */

    uint64_t instr_cnt;
};

#endif //YARISCVEMU_RV32_P_H
