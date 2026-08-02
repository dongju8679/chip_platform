#ifndef __PLIC_ENABLE__
#define __PLIC_ENABLE__

#define BP_MAX (1)

#define TEST_DEBUG_BASE_ADDR (0x80011000)
#define TEST_ADDR (TEST_DEBUG_BASE_ADDR)
#define TEST (*(volatile unsigned int *)(TEST_ADDR))

#define MAX_PLIC_NUMBER (45)

#endif