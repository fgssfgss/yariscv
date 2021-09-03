//
// Created by andrew on 8/9/21.
//

#ifndef YARISCVEMU_CLINT_H
#define YARISCVEMU_CLINT_H

#include <stdint.h>

void clint_init(void);

void clint_check_time(void);

uint64_t clint_mtime(void);

#endif //YARISCVEMU_CLINT_H
