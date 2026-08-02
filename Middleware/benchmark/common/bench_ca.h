#ifndef __BENCH_CA__
#define __BENCH_CA__

/* bench_ca.h - cycle/instret/CPI measurement over a benchmark region
 *              (additive only)
 *
 * Reuses the existing CA infrastructure (Platform/Common/Inc/verif_ca.h,
 * chip_docs/verif/docs/CA_measure.md) as is. The only new thing here is the
 * "benchmark report format".
 *   - verif_ca_get_cycle()   : mcycle/mcycleh (rv32 -> hi/lo/hi re-check loop)
 *   - verif_ca_get_instret() : minstret/minstreth
 *   - verif_ca_mark_cycle()  : refreshes the mirror for the host
 *                              (verif_host.h::get_cycle())
 *                              0x80010040/44 (cyc), 0x80010048/4C (instret)
 *
 * CPI uses integer division only (rv32 soft-float helpers cannot be linked).
 * -> CPI is computed as a 1000x fixed-point value (milli-CPI) and printed as
 *    "x.yyy".
 */

#include <stdint.h>
#include <stddef.h>
#include "verif_ca.h"
#include "bench_stdio.h"

/* The core clock that mcycle counts. DTS: clock-frequency = <500000000>
 * (500MHz). Do not confuse it with the CLINT MTIME timebase-frequency of
 * <500000> (500kHz). */
#ifndef BENCH_CPU_HZ
#define BENCH_CPU_HZ 500000000u
#endif

typedef struct {
    uint64_t cyc0, ins0;
    uint64_t cyc,  ins;
} bench_ca_t;

static inline void bench_ca_start(bench_ca_t *m)
{
    verif_ca_mark_cycle();                 /* host mirror: start of the region */
    m->ins0 = verif_ca_get_instret();
    m->cyc0 = verif_ca_get_cycle();
}

static inline void bench_ca_stop(bench_ca_t *m)
{
    m->cyc = verif_ca_get_cycle()   - m->cyc0;
    m->ins = verif_ca_get_instret() - m->ins0;
    verif_ca_mark_cycle();                 /* host mirror: end of the region */
}

/* u64 / u64 (there is no libgcc __udivdi3 -> bitwise shift-subtract) */
static inline uint64_t bench_div64(uint64_t n, uint64_t d, uint64_t *rem)
{
    uint64_t q = 0, r = 0;
    int i;
    if (d == 0) { if (rem) *rem = 0; return 0; }
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1u);
        if (r >= d) { r -= d; q |= (1ull << i); }
    }
    if (rem) *rem = r;
    return q;
}

/* Fixed-point division: (n * scale) / d as an integer.
 * Within this benchmark's range (n <= a few billion cycles, scale = 1000),
 * n*scale is on the order of 10^13, which is safe inside a u64. */
static inline uint32_t bench_fixdiv(uint64_t n, uint64_t d, uint32_t scale)
{
    if (d == 0) return 0;
    return (uint32_t)bench_div64(n * (uint64_t)scale, d, NULL);
}

/* Print "integer part.3 decimal places" (milli-unit fixed point) */
static inline void bench_print_milli(const char *label, uint32_t milli)
{
    printf("%s%u.%03u", label, milli / 1000u, milli % 1000u);
}

/* The shared benchmark CA report - CoreMark and Dhrystone use this format */
static inline void bench_ca_report(const char *tag, const bench_ca_t *m,
                                   uint32_t iterations)
{
    uint32_t cpi_m   = bench_fixdiv(m->cyc, m->ins ? m->ins : 1, 1000);
    uint32_t us_m    = bench_fixdiv(m->cyc, BENCH_CPU_HZ / 1000000u, 1000);
    uint64_t cyc_it  = iterations ? bench_div64(m->cyc, iterations, NULL) : 0;
    uint64_t ins_it  = iterations ? bench_div64(m->ins, iterations, NULL) : 0;

    printf("[CA] ---- %s : execution-driven cycle-accurate ----\n", tag);
    printf("[CA] cycles        : %lu\n",  (unsigned long)m->cyc);
    printf("[CA] instret       : %lu\n",  (unsigned long)m->ins);
    bench_print_milli("[CA] CPI           : ", cpi_m);  printf("\n");
    printf("[CA] iterations    : %u\n",   iterations);
    printf("[CA] cycles/iter   : %lu\n",  (unsigned long)cyc_it);
    printf("[CA] instret/iter  : %lu\n",  (unsigned long)ins_it);
    bench_print_milli("[CA] time @500MHz  : ", us_m);   printf(" us\n");
}

#endif /* __BENCH_CA__ */
