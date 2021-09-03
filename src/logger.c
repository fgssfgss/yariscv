//
// Created by andrew on 8/17/21.
//

#include "logger.h"
#include <stdarg.h>
#include <stdbool.h>

static FILE *log_output;
static bool disabled = true;

void logger_init(FILE *fp) {
    log_output = fp;
    disabled = false;
}

void logger_init_path(const char *path) {
    FILE *fp = fopen(path, "a+");

    if(fp == NULL) {
        printf("WTF????? CANNOT OPEN LOG FILE!!!\r\n");
        return;
    }

    log_output = fp;

    disabled = false;

    fprintf(log_output, "Start logger!\r\n");
}

void logger_disable(void) {
	disabled = true;
}

void logger(char *fmt, ...) {
    va_list arg;

    if (disabled) {
    	return;
    }

    va_start(arg, fmt);
    vfprintf(log_output, fmt, arg);
    va_end(arg);

    fflush(log_output);
}
