#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H
#include "global.h"
#include "../stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"
#define BITMAP_MASK 1

struct bitmap {
	uint32_t btmp_bytes_len;
	uint8_t *bits;
};

void bitmap_init(struct bitmap *btmp);
int bitmap_scan_test(struct bitmap *btmp, uint32_t bit_idx);
int bitmap_scan(struct bitmap *btmp, uint32_t cnt);
void bitmap_set(struct bitmap *, uint32_t, uint8_t);

#endif
