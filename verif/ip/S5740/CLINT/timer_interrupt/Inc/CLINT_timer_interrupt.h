#ifndef __CLINT_TIMER_INTERRUPT__
#define __CLINT_TIMER_INTERRUPT__

#define BP_MAX (50)

#define TEST_DEBUG_BASE_ADDR (0x80011000)
#define TEST_ADDR (TEST_DEBUG_BASE_ADDR)
#define TEST (*(volatile unsigned int *)(TEST_ADDR))

#endif
