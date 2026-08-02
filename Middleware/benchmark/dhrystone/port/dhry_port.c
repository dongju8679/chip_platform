/* dhry_port.c - the Dhrystone 2.1 porting layer
 *               (chip_platform / RV32RocketConfig)
 *
 * The benchmark body (dhrystone_main.c / dhrystone.c / dhrystone.h) was taken
 * unmodified from riscv-tests' Dhrystone 2.1 (the standard distribution that
 * splits dhry_1+dhry_2 into two files) (Middleware/benchmark/UPSTREAM.md5).
 *
 * -- Time measurement --------------------------------------------
 *   dhrystone.h already defines, in its `#elif defined(__riscv)` branch,
 *       HZ 1000000, Too_Small_Time 1, CLOCK_TYPE "rdcycle()"
 *       Start_Timer() Begin_Time = read_csr(mcycle)
 *   so it is already mcycle based as shipped - no modification needed.
 *
 *   PITFALL: that branch hardcodes HZ to 1,000,000, but mcycle runs at 500MHz.
 *   So the values the original prints must be read as:
 *       "Microseconds for one run"  -> actually "cycles per run"
 *       "Dhrystones per Second"     -> actually "Dhrystones/sec at 1MHz"
 *   Being a 1MHz-normalized value, it can in fact be used directly as a
 *   frequency-independent metric:
 *       DMIPS/MHz = Dhrystones_per_Second (the original output) / 1757
 *       DMIPS@500MHz = DMIPS/MHz * 500
 *   (1757 = the VAX 11/780's Dhrystones/sec, the definition of 1 DMIPS)
 *   The porting layer reprints this conversion explicitly to prevent
 *   misreading.
 *
 * -- setStats() ---------------------------------------------------
 *   The common riscv-tests benchmark hook. It is called as setStats(1) right
 *   before the measurement loop and setStats(0) right after. The 64-bit
 *   mcycle/minstret reads from the existing CA infrastructure (verif_ca.h) are
 *   hooked in here, which also yields CPI.
 *
 * -- Wrapping main -----------------------------------------------
 *   The Dhrystone body owns main(), so leaving it as is would make it
 *   impossible to append a report. Rather than editing the source,
 *   dhrystone_main.c alone is compiled separately with -Dmain=dhry_main_entry
 *   (the USE_BENCH=dhrystone branch in Makefile.VERIF), and the real main()
 *   here calls it and then prints the CA/DMIPS report.
 *   -> The upstream sources stay unmodified.
 */

#include "bench_stdio.h"
#include "bench_ca.h"
#include "dhrystone.h"

int dhry_main_entry(int argc, char **argv);

static bench_ca_t ca_run;
static int        ca_active;

/* The common riscv-tests hook (normally found in common/syscalls.c) */
void setStats(int enable)
{
    if (enable) {
        bench_ca_start(&ca_run);
        ca_active = 1;
    } else if (ca_active) {
        bench_ca_stop(&ca_run);
        ca_active = 0;
    }
}

int main(void)
{
    uint32_t runs = (uint32_t)NUMBER_OF_RUNS;
    uint32_t dhry_per_sec_1mhz;   /* = 1e6 * runs / cycles */
    uint32_t dmips_per_mhz_m;     /* milli fixed point */
    uint32_t dmips_500_m;

    printf("[bench] Dhrystone %s on RV32RocketConfig (rv32imac, 500MHz)\n",
           Version);
    printf("[bench] NUMBER_OF_RUNS=%lu  clock=mcycle (rdcycle branch)\n",
           (unsigned long)runs);

    dhry_main_entry(0, (char **)0);

    if (ca_run.cyc == 0) {
        printf("[bench] ERROR: no cycles measured (setStats hook not called?)\n");
        return 1;
    }

    bench_ca_report("Dhrystone", &ca_run, runs);

    /* Dhrystones/sec at 1MHz = iterations per 1e6 cycles */
    dhry_per_sec_1mhz =
        (uint32_t)bench_div64((uint64_t)runs * 1000000ull, ca_run.cyc, 0);

    /* DMIPS = Dhrystones/sec / 1757 */
    dmips_per_mhz_m = bench_fixdiv((uint64_t)runs * 1000000ull,
                                   ca_run.cyc * 1757ull, 1000);
    dmips_500_m     = bench_fixdiv((uint64_t)runs * 1000000ull * 500ull,
                                   ca_run.cyc * 1757ull, 1000);

    printf("[bench] ---- Dhrystone score ----\n");
    printf("[bench] Dhrystones/s @1MHz : %lu\n",
           (unsigned long)dhry_per_sec_1mhz);
    printf("[bench] Dhrystones/s @500MHz: %lu\n",
           (unsigned long)dhry_per_sec_1mhz * 500ul);
    bench_print_milli("[bench] DMIPS/MHz          : ", dmips_per_mhz_m);
    printf("\n");
    bench_print_milli("[bench] DMIPS @500MHz      : ", dmips_500_m);
    printf("\n");
    printf("[bench] (1 DMIPS = 1757 Dhrystones/s, VAX 11/780 reference)\n");
    printf("BENCH_DHRYSTONE_DONE\n");
    return 0;
}
