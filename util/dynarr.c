#include "dynarr.h"

static inline void da_safety(dynarr *da)
{
	if (da->elem_stride < 1)
		dbg_error("dynarr cannot have an element stride less than 1");
}

static inline void *da_last_elem(dynarr *da)
{
	return da->elems + da->elem_stride * (da->size - 1);
}

void da_init(dynarr *da, uint32_t elem_stride)
{
	da->elems = malloc(1);
	da->size = 0;
	da->elem_stride = elem_stride;

	da_safety(da);
}

void da_clean(dynarr *da)
{
	free(da->elems);

	da->size = 0;
	da->elem_stride = 0;
}

void da_add_elem(dynarr *da, void *elem)
{
	da->size++;

	da->elems = realloc(da->elems, da->size * da->elem_stride);
	memcpy((uint8_t *)da_last_elem(da), (uint8_t *)elem, da->elem_stride);
}

void *da_get(dynarr *da, uint32_t ind)
{
	return da->elems + da->elem_stride * ind;
}