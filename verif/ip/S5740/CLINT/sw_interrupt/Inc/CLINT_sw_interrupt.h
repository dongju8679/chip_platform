#ifndef __CLINT_SW_INTERRUPT__
#define __CLINT_SW_INTERRUPT__

#define BP_MAX (5)

#define TEST_DEBUG_BASE_ADDR (0x80011000)  /* chipyard mailbox BUF (was rt-dev 0x50000) */
#define TEST_ADDR (TEST_DEBUG_BASE_ADDR)
#define TEST (*(volatile unsigned int *)(TEST_ADDR))

#endif
