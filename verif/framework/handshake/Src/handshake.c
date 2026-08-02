/* handshake.c - (A) companion firmware demonstrating host_*bp round-trip
 *
 * Syncs with the host (verif_tsi_t::run_vseq, +verif=DEMO_handshake) via a DRAM mailbox:
 *   RESULT_ADDR <- 0xABCD     (produce result)
 *   BP_ADDR     <- BP_DONE(2) (signal done)
 *   wait until host sets BP_ADDR <- BP_RELEASE(3)  (released after host_check)
 *   exit(tohost)
 *
 * Addresses agreed with host (verif_host.h): BP_ADDR=0x80010000, RESULT_ADDR=0x80010004.
 * (RV32RocketConfig DRAM region. Firmware is M-mode, so it accesses absolute addresses directly.)
 */
#include "S5740_test.h"   /* exit(), CODE_SUCCESS */

#define BP_ADDR      0x80010000u
#define RESULT_ADDR  0x80010004u
#define RESULT_VAL   0x0000ABCDu
#define BP_DONE      2u
#define BP_RELEASE   3u

static inline void mem_fence(void){ __asm__ volatile("fence" ::: "memory"); }

int main(void)
{
    volatile unsigned int *bp  = (volatile unsigned int *)BP_ADDR;
    volatile unsigned int *res = (volatile unsigned int *)RESULT_ADDR;

    *bp  = 0u;             /* init mailbox */
    *res = RESULT_VAL;     /* produce result */
    mem_fence();
    *bp  = BP_DONE;        /* signal "done" to host */
    mem_fence();

    /* wait until host releases (BP_RELEASE) after checking (with safety timeout) */
    unsigned int guard = 0u;
    while (*bp != BP_RELEASE) {
        if (++guard > 200000000u) break;   /* avoid infinite loop if host never comes */
    }

    exit(CODE_SUCCESS);    /* tohost = 1 */
    return CODE_SUCCESS;
}
