/* What shim.c offers dpu.c: the console and the one way a store reaches a device. */
#ifndef SHIM_SHIM_H
#define SHIM_SHIM_H

#include <stdint.h>

void say(const char *fmt, ...);                         /* %s %x %d, 64-bit arguments */
int mmio_write32(uint64_t addr, uint32_t value);        /* 0 if taken; -1 if the address is not on the allow list */
void ring_flush(void);                                  /* the log so far, to the RAM ring (dpu.h) */

#endif
