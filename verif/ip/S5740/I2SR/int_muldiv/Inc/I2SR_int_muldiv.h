#ifndef __I2SR_INT_MULDIV__
#define __I2SR_INT_MULDIV__
/* I2SR_int_muldiv.h - rt-dev original, addresses remapped to chipyard RV32Rocket.
 *
 * [Port policy] The .c/.cc test bodies are kept byte-for-byte identical to rt-dev.
 *               Only this header changes (S5740 addresses -> chipyard addresses).
 *
 *   rt-dev (S5740)            chipyard (RV32Rocket)
 *   SW_INT     0x2000    ->   0x02000000  CLINT MSIP
 *   MTIMECMP   0x2100    ->   0x02004000  CLINT MTIMECMP (64-bit)
 *   MTIME      0x2B00    ->   0x0200BFF8  CLINT MTIME
 *   TEST_ADDR  0x50000   ->   0x80010004
 *   BREAK_ADDR 0x51000   ->   0x80010000
 *   BUF_ADDR   0x52000   ->   0x80011000  (DRAM, host reads/writes directly)
 *   PLIC       0x4000..  ->   0x0C000000  (see PLIC_* below)
 */

#define BP_MAX (1)

#define TEST_DEBUG_BASE_ADDR (0x80010004)
#define TEST_ADDR (TEST_DEBUG_BASE_ADDR)
#define TEST (*(volatile unsigned int *)(TEST_ADDR))

#define I2SR_ENABLE {asm volatile("csrwi 0x7c2, 1");}
#define SW_INT   (0x02000000)      /* CLINT MSIP     (rt-dev 0x2000) */
#define MTIMECMP (0x02004000)      /* CLINT MTIMECMP (rt-dev 0x2100) */
#define MTIME    (0x0200BFF8)      /* CLINT MTIME    (rt-dev 0x2B00) */

#define MAX_TASKNUM (5)
#define OUTER_ITERATION (1)
#define INNER_ITERATION (1)
#define BREAK_ADDR (0x80010008)   /* exit flag - MUST differ from BP addr(0x80010000) */
#define BUF_SIZE (0x40)
struct BUF {
unsigned int min_time;
unsigned int max_time;
unsigned int tot_time;
unsigned int count;
unsigned int data[BUF_SIZE][2];
};
#define BUF_ADDR (0x80011000)

#endif
