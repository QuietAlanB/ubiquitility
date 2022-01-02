#ifndef DYNARR_H_INCLUDED
#define DYNARR_H_INCLUDED

#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define ARRAY_SIZE(a)				(sizeof(a) / sizeof(a[0]))

typedef struct {
	void *elems;
	uint32_t size;
	uint32_t elem_stride;
} dynarr;

void da_init(dynarr *da, uint32_t elem_stride);

void da_clean(dynarr *da);

void da_add_elem(dynarr *da, void *elem);

void *da_get(dynarr *da, uint32_t ind);

#endif