//
// Created by andrew on 8/9/21.
//

#ifndef YARISCVEMU_CLINT_H
#define YARISCVEMU_CLINT_H

#include <stdint.h>

void clint_init(void);
void clint_iter(uint32_t *mip);

#endif //YARISCVEMU_CLINT_H
