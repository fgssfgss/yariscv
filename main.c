#include <stdio.h>
#include "src/rv32.h"
#include "src/memmap.h"
#include "src/loader.h"
#include "src/uart.h"
#include "src/plic.h"
#include "src/clint.h"
#include "src/logger.h"

#define RAM_BASE (0x80000000)
#define RAM_SIZE (1024 * 1024 * 1024)
#define LOWMEM_BASE (0x00000000)
#define LOWMEM_SIZE (0x00010000)

int main(int argc, char *argv[]) {
    logger_init_path("/tmp/output.txt");

    /* TODO: do getopt() here */
    if (argc < 2) {
        printf("Not enough arguments!\r\n");
        return -1;
    }

    //logger_disable();

    memmap_init();
    memmap_append(LOWMEM_BASE, LOWMEM_BASE + LOWMEM_SIZE, RAM, NULL, NULL);
    memmap_append(RAM_BASE, RAM_BASE + RAM_SIZE, RAM, NULL, NULL);

    uart_init();
    plic_init();
    clint_init();

    if (!loader_load("./machine.dtb", memmap_get(0x00001000), 0x00001000)) {
        printf("dtb file problem!!!1\r\n");
        return 1;
    }

    if (!loader_load(argv[1], memmap_get(0x80000000), 0x00000000)) {
        printf("cannot open or load a file\r\n");
        return 1;
    }

    rv32_init();

    rv32_run_until();

    return 0;
}
