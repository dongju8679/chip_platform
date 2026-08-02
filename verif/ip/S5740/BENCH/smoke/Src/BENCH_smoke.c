/* BENCH_smoke.c - minimal test that checks the HTIF console path (benchmark pre-flight) */
#include "bench_htif.h"
#include "verif_mbx_base.h"
#include "verif_ca.h"
#include "util.h"

int main(void)
{
    uint64_t c0 = verif_ca_get_cycle();
    htif_puts("HTIF_OK\n");
    uint64_t c1 = verif_ca_get_cycle();

    /* Record cycles-per-character into the mailbox (UART unused) */
    *(volatile uint32_t *)(VERIF_MBX_BASE + VERIF_MBX_OFF_SMOKE) = (uint32_t)(c1 - c0);
    verif_ca_mark_cycle();

    htif_puts("SMOKE_DONE\n");
    exit(CODE_SUCCESS);
    return 0;
}
