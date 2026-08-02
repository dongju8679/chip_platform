#ifndef __UTIL__
#define __UTIL__

#include "type.h"
#include "encoding.h"
void *memset(void *, int, unsigned int);
void *memcpy(void *, const void *, unsigned int);

#define outpdw(a, d) (*(volatile uint32_t *)(a) = (uint32_t)(d))
#define inpdw(a) (*(volatile uint32_t *)(a))

typedef struct _LOG{
unsigned int start_time;
unsigned int end_time;
unsigned int run_time;

}LOG;

extern LOG log_test;
unsigned int set_log(unsigned int i);

#if defined(USE_NEWLIB_STDIO) && USE_NEWLIB_STDIO
/* Platform/Common/Src/newlib_glue.c - make stdout unbuffered (called from set_log(1)) */
void newlib_stdio_init(void);
#endif
#define READ_MTVAL (read_csr(mtval))
#define WRITE_MTVAL(d) (write_csr(mtval, d))

#include "verif_mbx_base.h"
#ifndef BREAK_POINT_ADDR
#define BREAK_POINT_ADDR (VERIF_MBX_BASE + VERIF_MBX_OFF_BREAK)
#endif  /* chipyard host-DUT mailbox (was rt-dev 0x10000) */
#define BREAK_POINT (*(volatile unsigned int *)(BREAK_POINT_ADDR))

#define BP_MEM_WRITE (0xFFFFFFF0)
#define BP_TEST_init (0xFFFFFFF1)
#define BP_TEST_begin (0xFFFFFFF2)
#define BP_TEST_success (0xFFFFFFF3)
#define BP_TEST_fail (0xFFFFFFF4)
#define BP_TEST_end (0xFFFFFFF5)
#define BP_END (0xFFFFFFFF)

void bp_init(void);
void dut_set_bp(unsigned int);
unsigned int dut_wait_bp(unsigned int);
#define CODE_SUCCESS 0x0
#define CODE_FAIL 0x1
#define CODE_FATAL 0x2

void exit(unsigned int);

#endif