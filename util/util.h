#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

size_t get_file_len(char *filepath);

void read_file(char *dest, char *filepath);

uint32_t clamp_uint(uint32_t val, uint32_t min, uint32_t max);

#endif