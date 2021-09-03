//
// Created by andrew on 9/8/21.
//

#ifndef YARISCVEMU_YARISCVEMU_H
#define YARISCVEMU_YARISCVEMU_H

#include <stdbool.h>

int yariscvemu_init(int argc, char *argv[]);

void yariscvemu_run(void);

void yariscvemu_running(bool run);

#endif //YARISCVEMU_YARISCVEMU_H
