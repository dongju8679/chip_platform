/* core_portme.c - the CoreMark porting layer (chip_platform / RV32RocketConfig)
 *
 * This fills in the #error slots of EEMBC barebones/core_portme.c for this
 * target. The CoreMark body itself is upstream and unmodified.
 *
 * -- Time measurement: mcycle -----------------------------------
 *   This platform has neither an OS nor an RTC library. Instead it reuses the
 *   mcycle reads of the existing CA infrastructure
 *   (Platform/Common/Inc/verif_ca.h, chip_docs/verif/docs/CA_measure.md) as is.
 *   On RTL (Verilator) mcycle is the real core clock including pipeline stalls
 *   and cache misses, so it is exactly right as CoreMark's "ticks".
 *   minstret over the same region is read alongside it to produce CPI.
 *
 *   CORETIMETYPE is ee_u32 (the low 32 bits of mcycle), which covers 8.5 s at
 *   500MHz. RTL simulation runs at a few thousand cycles/s, so an 8.5-second
 *   run (= 4.29 billion cycles) is not something that can be executed at all
 *   -> overflow is impossible.
 *   Even so, the porting layer records a separate 64-bit counter (including
 *   mcycleh) in parallel and uses it for the [CA] report.
 *
 * -- Score computation: why it is printed here rather than by CoreMark --
 *   With HAS_FLOAT=0, CoreMark's `Iterations/Sec : %f` path is disabled, and on
 *   the integer path time_in_secs() returns whole seconds, which is 0 for short
 *   runs.
 *   So the porting layer computes it directly from cycles:
 *
 *       CoreMark/MHz = iterations * 1e6 / total_cycles
 *
 *   (Derivation: score = iter/sec = iter*f/cycles; dividing that by f/1e6 [MHz]
 *    gives iter*1e6/cycles - the frequency cancels out, making the value clock
 *    independent.)
 *   CoreMark scaled to 500MHz = CoreMark/MHz * 500.
 *
 * -- Versus the official run rules --------------------------------
 *   The EEMBC rules require "at least 10 seconds of execution". At 500MHz, 10 s
 *   is 5 billion cycles, which would take weeks at Verilator speed and is
 *   physically impossible on RTL.
 *   So the value produced here is not a "rule-compliant official score" but
 *   "CoreMark/MHz measured on cycle-accurate RTL". This is stated in the
 *   documentation.
 *   The CRC checks (seedcrc/crclist/crcmatrix/crcstate/crcfinal) are still
 *   performed in full, so computational correctness is completely verified.
 */

#include "coremark.h"
#include "core_portme.h"
#include "bench_stdio.h"
#include "bench_ca.h"

#if VALIDATION_RUN
volatile ee_s32 seed1_volatile = 0x3415;
volatile ee_s32 seed2_volatile = 0x3415;
volatile ee_s32 seed3_volatile = 0x66;
#endif
#if PERFORMANCE_RUN
volatile ee_s32 seed1_volatile = 0x0;
volatile ee_s32 seed2_volatile = 0x0;
volatile ee_s32 seed3_volatile = 0x66;
#endif
#if PROFILE_RUN
volatile ee_s32 seed1_volatile = 0x8;
volatile ee_s32 seed2_volatile = 0x8;
volatile ee_s32 seed3_volatile = 0x8;
#endif
volatile ee_s32 seed4_volatile = ITERATIONS;
volatile ee_s32 seed5_volatile = 0;

/* -- Timer --------------------------------------------------- */
CORETIMETYPE
barebones_clock(void)
{
    return (CORETIMETYPE)verif_ca_mcycle_lo();
}

#define GETMYTIME(_t)              (*_t = barebones_clock())
#define MYTIMEDIFF(fin, ini)       ((fin) - (ini))
#define TIMER_RES_DIVIDER          1
#define SAMPLE_TIME_IMPLEMENTATION 1
#define EE_TICKS_PER_SEC           (BENCH_CPU_HZ / TIMER_RES_DIVIDER)

static CORETIMETYPE start_time_val, stop_time_val;

/* Parallel 64-bit cycle/instret recording (for the CA report) */
static bench_ca_t ca_run;

void
start_time(void)
{
    bench_ca_start(&ca_run);
    GETMYTIME(&start_time_val);
}

void
stop_time(void)
{
    GETMYTIME(&stop_time_val);
    bench_ca_stop(&ca_run);
}

CORE_TICKS
get_time(void)
{
    return (CORE_TICKS)(MYTIMEDIFF(stop_time_val, start_time_val));
}

/* With HAS_FLOAT=0, secs_ret is an ee_u32 (whole seconds).
 * It being 0 on short RTL runs is normal, and that fact is itself the signal
 * that "the official 10-second rule was not met". We do not round it up to
 * disguise that. */
secs_ret
time_in_secs(CORE_TICKS ticks)
{
    return (secs_ret)((ee_u32)ticks / (ee_u32)EE_TICKS_PER_SEC);
}

ee_u32 default_num_contexts = 1;

void
portable_init(core_portable *p, int *argc, char *argv[])
{
    (void)argc;
    (void)argv;

    ee_printf("[bench] CoreMark on RV32RocketConfig (rv32imac, 500MHz)\n");
    ee_printf("[bench] ITERATIONS=%d TOTAL_DATA_SIZE=%d MEM=%s\n",
              (int)ITERATIONS, (int)TOTAL_DATA_SIZE, MEM_LOCATION);

    if (sizeof(ee_ptr_int) != sizeof(ee_u8 *))
        ee_printf("ERROR! ee_ptr_int size mismatch\n");
    if (sizeof(ee_u32) != 4)
        ee_printf("ERROR! ee_u32 is not 32b\n");

    p->portable_id = 1;
}

void
portable_fini(core_portable *p)
{
    /* CoreMark/MHz = iterations * 1e6 / cycles  (frequency independent)
     * Computed in milli fixed point and printed to 3 decimal places. */
    ee_u32 iters = (ee_u32)ITERATIONS;
    uint32_t cm_per_mhz_m =
        bench_fixdiv((uint64_t)iters * 1000000ull, ca_run.cyc, 1000);
    uint32_t cm_at_500_m =
        bench_fixdiv((uint64_t)iters * 1000000ull * 500ull, ca_run.cyc, 1000);

    bench_ca_report("CoreMark", &ca_run, iters);

    ee_printf("[bench] ---- CoreMark score ----\n");
    bench_print_milli("[bench] CoreMark/MHz      : ", cm_per_mhz_m);
    ee_printf("\n");
    bench_print_milli("[bench] CoreMark @500MHz  : ", cm_at_500_m);
    ee_printf("\n");
    ee_printf("[bench] NOTE: EEMBC run rules require >=10s of execution.\n");
    ee_printf("[bench] RTL sim cannot reach that (10s @500MHz = 5e9 cycles).\n");
    ee_printf("[bench] CRC validation above is the correctness proof.\n");
    ee_printf("BENCH_COREMARK_DONE\n");

    p->portable_id = 0;
}
