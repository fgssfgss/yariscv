//
// Created by andrew on 9/8/21.
//

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include "yariscvemu.h"
#include "rv32.h"
#include "memmap.h"
#include "loader.h"
#include "uart.h"
#include "plic.h"
#include "clint.h"
#include "logger.h"
#include "console.h"

#define RAM_BASE (0x80000000)
#define RAM_SIZE (1024 * 1024 * 256)
#define LOWMEM_BASE (0x00000000)
#define LOWMEM_SIZE (0x00010000)
#define DTB_OFFSET (0x00001000)
#define FW_OFFSET (0x80000000)
#define INITRD_OFFSET (0x82800000)

static volatile bool running = false;
static char *fw_path = NULL;
static char *dtb_path = NULL;
static char *initrd_path = NULL;
#ifndef NDEBUG
static char *log_path = NULL;
static bool enable_log = false;
#endif

static void yariscvemu_parse_arguments(int argc, char *argv[]);

int yariscvemu_init(int argc, char *argv[]) {
	yariscvemu_parse_arguments(argc, argv);

#ifndef NDEBUG
	if (enable_log) {
		logger_init_path(log_path);
	}
#endif

	console_init();

	memmap_init();
	memmap_append(RAM_BASE, RAM_BASE + RAM_SIZE, RAM, NULL, NULL);
	memmap_append(LOWMEM_BASE, LOWMEM_BASE + LOWMEM_SIZE, RAM, NULL, NULL);

	uart_init();
	plic_init();
	clint_init();
	rv32_init();

	if (!loader_load(dtb_path, DTB_OFFSET)) {
		printf("Cannot open or load DTB file at %s\r\n", dtb_path);
		return 1;
	}

	if (!loader_load(fw_path, FW_OFFSET)) {
		printf("Cannot open or load a firmware file at %s\r\n", fw_path);
		return 1;
	}

	if (!loader_load(initrd_path, INITRD_OFFSET)) {
		printf("Cannot open or load an initrd file at %s\r\n", initrd_path);
		return 1;
	}

	return 0;
}

void yariscvemu_running(bool run) {
	running = run;
}

void yariscvemu_run(void) {
	while (running) {
		rv32_run(50000);
		console_read();
		clint_check_time();
		rv32_raise_interrupts();
	}
}

static void yariscvemu_parse_arguments(int argc, char *argv[]) {
	const char *short_options = "hf::i::d::l::";

	const struct option long_options[] = {
			{"help", no_argument,       NULL, 'h'},
			{"firmware", optional_argument, NULL, 'f'},
			{"initrd", optional_argument, NULL, 'i'},
			{"dtb", optional_argument, NULL, 'd'},
#ifndef NDEBUG
			{"logger", optional_argument, NULL, 'l'},
#endif
			{NULL, 0,                   NULL, 0}
	};

	int rez;
	int option_index;

	while ((rez = getopt_long(argc, argv, short_options,
	                          long_options, &option_index)) != -1) {

		switch (rez) {
			case 'h': {
				printf("USAGE: %s [-f|--firmware] [-i|--initrd][-d|--dtb][-h|--help]\n", argv[0]);
				exit(0);
			}
			case 'f': {
				if (optarg != NULL) {
					fw_path = optarg;
				}
				break;
			}
			case 'i': {
				if (optarg != NULL) {
					initrd_path = optarg;
				}
				break;
			}
			case 'd': {
				if (optarg != NULL) {
					dtb_path = optarg;
				}
				break;
			}
#ifndef NDEBUG
			case 'l': {
				enable_log = true;
				if (optarg != NULL) {
					log_path = optarg;
				}
				break;
			}
#endif
			case '?':
			default: {
				printf("Found unknown option. Exiting...\n");
				break;
			}
		};
	}

	if (fw_path == NULL) {
		fw_path = "fw.bin";
	}

	if (dtb_path == NULL) {
		dtb_path = "machine.dtb";
	}

	if (initrd_path == NULL) {
		initrd_path = "initramfs.cpio.gz";
	}

#ifndef NDEBUG
	if (log_path == NULL) {
		log_path = "/mnt/tmpfs-folder/out.txt";
	}
#endif
}
