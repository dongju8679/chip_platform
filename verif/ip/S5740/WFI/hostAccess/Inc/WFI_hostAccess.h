#ifndef __WFI_HOSTACCESS__
#define __WFI_HOSTACCESS__

#define BP_MAX (1)

#include "verif_mbx_base.h"
#define TEST_DEBUG_BASE_ADDR (VERIF_MBX_BASE + VERIF_MBX_OFF_TEST_DEBUG)
#define TEST_ADDR (TEST_DEBUG_BASE_ADDR)
#define TEST (*(volatile unsigned int *)(TEST_ADDR))

#define TEST_ITIM_BASE (0x80008000)
#define TEST_DTIM_BASE (0x80012000)
#define TEST_MEM_BASE (0x80012000)

#define TEST_MEM_SIZE (0x1000)
#define ITERATION (1000)

#endif
