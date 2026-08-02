#ifndef __SYSCALLS__
#define __SYSCALLS__

#define outpdw(a, d) (*(volatile uint32_t *)(a) = (uint32_t)(d))
#define inpdw(a) (*(volatile uint32_t *)(a))

#include <stdint.h>
xlen_t trap_handler(xlen_t);
int init_fpu();
#endif