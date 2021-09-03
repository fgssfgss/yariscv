//
// Created by andrew on 8/17/21.
//

#ifndef YARISCVEMU_LOGGER_H
#define YARISCVEMU_LOGGER_H

#include <stdio.h>

void logger_init(FILE *fp);
void logger_init_path(const char *path);
void logger_disable(void);
void logger(char *fmt, ...);

#endif //YARISCVEMU_LOGGER_H
