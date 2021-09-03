//
// Created by andrew on 8/2/21.
//

#ifndef YARISCVEMU_LOADER_H
#define YARISCVEMU_LOADER_H

#include <stdbool.h>
#include "memmap.h"

bool loader_load(const char *filename, memmap_entry_t *out, uintptr_t offset);

#endif //YARISCVEMU_LOADER_H
