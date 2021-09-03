//
// Created by andrew on 9/8/21.
//

#ifndef YARISCVEMU_CONSOLE_H
#define YARISCVEMU_CONSOLE_H

#include <stdint.h>

void console_init(void);

void console_read(void);

uint8_t console_get_char(void);

long console_is_empty(void);

#endif //YARISCVEMU_CONSOLE_H
