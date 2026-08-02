#ifndef __CLINT_RTC__
#define __CLINT_RTC__

#define BP_MAX (1)

#define TEST_DEBUG_BASE_ADDR (0x50000)
#define TEST_ADDR (TEST_DEBUG_BASE_ADDR)
#define TEST (*(volatile unsigned int *)(TEST_ADDR))

#endif
