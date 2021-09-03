//
// Created by andrew on 7/28/21.
//

#ifndef YARISCVEMU_MEMMAP_H
#define YARISCVEMU_MEMMAP_H

#include <stdint.h>

typedef uint32_t (*read_func_t)(uint32_t address, int size);
typedef void (*write_func_t)(uint32_t address, uint32_t data, int size);

typedef enum {
    RAM,
    DEVICE
} memtype;

typedef struct memmap_entry_s {
    uint32_t start;
    uint32_t end;

    memtype type;

    /* HANDLE READ/WRITE FUNCTIONS */
    union {
        struct {
            read_func_t read_handler;
            write_func_t write_handler;
        };
        uint8_t *phys_mem;
    };
} memmap_entry_t;

void memmap_init(void);

void memmap_append(uint32_t start, uint32_t end, memtype type, read_func_t read, write_func_t write);

memmap_entry_t *memmap_get(uint32_t address);

// TODO: remove and rework this crap, we shouldn't be able to access iomem using unaligned addresses
static uint32_t inline read_unaligned_reg(uint32_t offset, uint32_t reg, int size) {
    switch(size) {
        case 1:
            return (reg >> (offset * 8)) & 0xff;
        case 2:
            return (reg >> (offset * 8)) & 0xffff;
        case 4:
            return reg;
        default:
            // WTF???
            return reg;
    }
}

static void inline write_unaligned_reg(uint32_t offset, uint32_t val, uint32_t *reg, int size) {
    switch(size) {
        case 1:
            *reg &= ~(0xff << (offset * 8));
            *reg |= ((val & 0xff) << (offset * 8));
            break;
        case 2:
            *reg &= ~(0xffff << (offset * 8));
            *reg |= ((val & 0xffff) << (offset * 8));
            break;
        case 4:
            *reg = val;
            break;
        default:
            // WTF???
            *reg = val;
            break;
    }
}

#endif //YARISCVEMU_MEMMAP_H
