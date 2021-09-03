//
// Created by andrew on 8/17/21.
//

#ifndef YARISCVEMU_LOGGER_H
#define YARISCVEMU_LOGGER_H

#include <stdio.h>

void logger_init_path(const char *path);

#ifndef NDEBUG
#define logger(fmt, ...) logger_real(fmt, ##__VA_ARGS__)
#else
#define logger(fmt, ...)
#endif

void logger_real(char *fmt, ...);

#endif //YARISCVEMU_LOGGER_H
