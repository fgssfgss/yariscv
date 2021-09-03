//
// Created by andrew on 8/17/21.
//

#include "logger.h"
#include <stdarg.h>
#include <stdbool.h>

static FILE *log_output;
static bool enabled = false;

void logger_init_path(const char *path) {
	FILE *fp = fopen(path, "a+");

	if (fp == NULL) {
		printf("CANNOT OPEN LOG FILE!\r\n");
		return;
	}

	log_output = fp;

	enabled = true;

	fprintf(log_output, "Start logger!\r\n");
}

void logger_real(char *fmt, ...) {
	va_list arg;

	if (!enabled) {
		return;
	}

	va_start(arg, fmt);
	vfprintf(log_output, fmt, arg);
	va_end(arg);

	fflush(log_output);
}
