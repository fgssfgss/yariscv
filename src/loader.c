//
// Created by andrew on 8/2/21.
//

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "loader.h"

static uint64_t loader_get_filesize(FILE *fp);

bool loader_load(const char *filename, memmap_entry_t *out, uintptr_t offset) {
    if (out == NULL || out->type != RAM) {
        return false;
    }

    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        return false;
    }

    uint64_t sz = loader_get_filesize(fp);
    if (sz == 0 || sz > (out->end - out->start)) {
        return false;
    }

    fread((void *)(out->phys_mem + offset), sz, sizeof(char), fp);

    return true;
}

static uint64_t loader_get_filesize(FILE *fp) {
    uint64_t sz = 0;
    fseek(fp, 0L, SEEK_END);
    sz = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    return sz;
}