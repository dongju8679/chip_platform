/* boot.c - boot sequence checks + the C-side initialization entry point
 *
 * See Platform/Common/Inc/boot.h for the full boot order and design rationale.
 */
#include "boot.h"
#include "data_init.h"
#include "exceptions.h"
#include "uart.h"
#include "encoding.h"

/* Symbols supplied by link.ld (the address itself is the value) */
extern char __stack_top[];
extern char __heap_start[];

/* -- Canary for the .bss check ---------------------------------------
 * Why boot_check() does not use data_init_bss_is_zero():
 *   crt.S clears .bss and then calls set_log(1) before main. That function
 *   writes mcycle into the global log_test (in .bss) in util.c. So by the time
 *   main is entered .bss is already non-zero - a whole-range check would always
 *   fail.
 *   Instead we keep one dedicated array that nobody ever touches and check only
 *   that.
 *
 * Limitation of this check (stated honestly):
 *   If the simulator's DRAM is zero-filled at reset, this check passes even if
 *   the clear in crt.S were removed entirely. In other words it does not prove
 *   "the clear happened", only "the clear is not broken". Proving it properly
 *   would require a host-side preload that dirties .bss beforehand (a follow-up
 *   task).
 */
static volatile unsigned int boot_bss_canary[8];

static xlen_t read_sp(void)
{
    xlen_t v;
    __asm__ volatile ("mv %0, sp" : "=r"(v));
    return v;
}

static xlen_t read_gp(void)
{
    xlen_t v;
    __asm__ volatile ("mv %0, gp" : "=r"(v));
    return v;
}

void boot_get_info(boot_info_t *info)
{
    if (!info) return;
    info->sp        = read_sp();
    info->gp        = read_gp();
    info->mtvec     = read_csr(mtvec);
    info->stack_top = (xlen_t)(unsigned long)__stack_top;
    info->heap_start= (xlen_t)(unsigned long)__heap_start;
    data_init_bss_range(&info->bss_start, &info->bss_end);
}

unsigned boot_check(void)
{
    boot_info_t bi;
    unsigned err = 0;

    boot_get_info(&bi);

    /* sp grows downward from __stack_top. If it has dropped below heap_start,
     * the stack has run into .bss/heap (the 60K ram in link.ld is tight). */
    if (bi.sp > bi.stack_top || bi.sp <= bi.heap_start) err |= BOOT_ERR_SP;

    /* With gp unset, every %gp-relative access (.sdata/.sbss) hits the wrong
     * place. */
    if (bi.gp == 0) err |= BOOT_ERR_GP;

    /* mtvec: if 0, a trap jumps to address 0 and dies.
     * The low 2 bits are the MODE field - 0=Direct. crt.S installs Direct. */
    if (bi.mtvec == 0 || (bi.mtvec & 0x3u) != 0) err |= BOOT_ERR_MTVEC;

    {
        unsigned i;
        for (i = 0; i < sizeof(boot_bss_canary)/sizeof(boot_bss_canary[0]); i++) {
            if (boot_bss_canary[i] != 0u) { err |= BOOT_ERR_BSS; break; }
        }
    }

    return err;
}

void boot_init_hal(void)
{
    uart_init();        /* txen is 0 at reset - printf stalls unless it is enabled */
    exceptions_init();  /* replace mtvec with _trap_entry_common */
}
