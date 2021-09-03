//
// Created by andrew on 8/9/21.
//

#include <stdio.h>
#include "clint.h"
#include "memmap.h"
#include "rv32.h"

#define CLINT_BASE_ADDR 0x2000000
#define CLINT_WINDOW    0x10000

#define MSIP_BASE        (CLINT_BASE_ADDR + 0x0)
#define MSIP_WINDOW      (MSIP_BASE + 0x4)

#define MTIMEL_BASE      (CLINT_BASE_ADDR + 0xbff8)
#define MTIMEL_WINDOW    (MTIMEL_BASE + 0x4)

#define MTIMEH_BASE      (CLINT_BASE_ADDR + 0xbffc)
#define MTIMEH_WINDOW    (MTIMEH_BASE + 0x4)

#define MTIMECMPL_BASE   (CLINT_BASE_ADDR + 0x4000)
#define MTIMECMPL_WINDOW (MTIMECMPL_BASE + 0x4 - 1)

#define MTIMECMPH_BASE   (CLINT_BASE_ADDR + 0x4004)
#define MTIMECMPH_WINDOW (MTIMECMPH_BASE + 0x4)

static uint32_t clint_read_handler(uint32_t address, int size);
static void clint_write_handler(uint32_t address, uint32_t data, int size);

static union {
    uint64_t mtime;
    struct {
        uint32_t mtimel;
        uint32_t mtimeh;
    };
} mtime;

static union {
    uint64_t mtimecmp;
    struct {
        uint32_t mtimecmpl;
        uint32_t mtimecmph;
    };
} mtimecmp;

static uint32_t msip;

void clint_init(void) {
    memmap_append(CLINT_BASE_ADDR, CLINT_BASE_ADDR + CLINT_WINDOW, DEVICE, clint_read_handler, clint_write_handler);
    printf("CLINT inited\r\n");
}

void clint_iter(uint32_t *mip) {
    mtime.mtime++;

    if (msip & 1) {
        *mip |= (MIP_MSIP | MIP_SSIP);
    }

    if (mtime.mtime >= mtimecmp.mtimecmp) {
        *mip |= (MIP_MTIP | MIP_STIP);
    }

    if (mtimecmp.mtimecmp > mtime.mtime) {
        *mip &= ~(MIP_MTIP | MIP_STIP);
    }
}

static uint32_t clint_read_handler(uint32_t address, int size) {
    switch(address) {
        case MSIP_BASE ... MSIP_WINDOW - 1: {
            return read_unaligned_reg(address - MSIP_BASE, msip, size);
        }
        case MTIMECMPL_BASE ... MTIMECMPL_WINDOW - 1: {
            return read_unaligned_reg(address - MTIMECMPL_BASE, mtimecmp.mtimecmpl, size);
        }
        case MTIMECMPH_BASE ... MTIMECMPH_WINDOW - 1: {
            return read_unaligned_reg(address - MTIMECMPH_BASE, mtimecmp.mtimecmph, size);
        }
        case MTIMEL_BASE ... MTIMEL_WINDOW - 1: {
            return read_unaligned_reg(address - MTIMEL_BASE, mtime.mtimel, size);
        }
        case MTIMEH_BASE ... MTIMEH_WINDOW - 1: {
            return read_unaligned_reg(address - MTIMEH_BASE, mtime.mtimeh, size);
        }
        default: {
            return 0;
        }
    }
}

static void clint_write_handler(uint32_t address, uint32_t data, int size) {
    switch(address) {
        case MSIP_BASE ... MSIP_WINDOW - 1: {
            write_unaligned_reg(address - MSIP_BASE, data, &msip, size);
        }
        case MTIMECMPL_BASE ... MTIMECMPL_WINDOW - 1: {
            write_unaligned_reg(address - MTIMECMPL_BASE, data, &mtimecmp.mtimecmpl, size);
        }
        case MTIMECMPH_BASE ... MTIMECMPH_WINDOW - 1: {
            write_unaligned_reg(address - MTIMECMPH_BASE, data, &mtimecmp.mtimecmph, size);
        }
        default: {
            // I don't know what to do here
        }
    }
}
