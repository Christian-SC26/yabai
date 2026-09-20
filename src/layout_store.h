#ifndef LAYOUT_STORE_H
#define LAYOUT_STORE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

bool layout_store_save(uint64_t sid, const char *name_or_path, char *out_path, size_t out_path_size, char *err_msg, size_t err_size);
bool layout_store_dump(uint64_t sid, FILE *rsp, char *err_msg, size_t err_size);
bool layout_store_restore(uint64_t sid, const char *name_or_path, char *out_path, size_t out_path_size, char *err_msg, size_t err_size);

#endif
