/* CA_measure.c - execution-driven cycle-accurate cycle measurement (v301)
 *
 * Running:
 *   verif/ip/S5740/CA/measure/run.sh
 *   = build the firmware (-DPRELOAD, OPT=-O2), then
 *     simulator ... +verif=CA_measure ... build/CA_measure.elf
 *
 * Measurement principle
 *   Read mcycle / minstret before and after each block and take the delta.
 *   - mcycle   : the core clock. The "real cycles", including all stalls,
 *                bubbles and cache misses.
 *   - minstret : committed instruction count. A value a commit log can also
 *                produce.
 *   - CPI = delta(mcycle) / delta(minstret) -> everything above 1.0 is stall.
 *
 *   Each block is called twice: the first run warms the I$ (not measured), only
 *   the second is measured. The measurement overhead (4 csrr plus the indirect
 *   call) is extracted separately as the empty OVERHEAD block, and the host
 *   compensates for it when displaying.
 *
 * Countermeasures against -O2
 *   Everything measured is inline asm (volatile + "memory" clobber), and the
 *   block functions are noinline, so the compiler can neither delete nor
 *   reorder them.
 *   Wrapping them in .option norvc keeps compressed (2-byte) instructions out
 *   and pins the instruction count.
 */

#include "type.h"
#include "util.h"
#include "CA_measure.h"

#ifdef CA_UART
#include "uart.h"
#include "printf.h"
#endif

/* -- CA result table (0x80010100) ----------------------------- */
#define CA_TBL_MAGIC (*(volatile uint32_t *)(VERIF_CA_TABLE_ADDR + 0))
#define CA_TBL_COUNT (*(volatile uint32_t *)(VERIF_CA_TABLE_ADDR + 4))
#define CA_TBL_CYC0  (*(volatile uint32_t *)(VERIF_CA_TABLE_ADDR + 8))   /* starting mcycle */
#define CA_TBL_INS0  (*(volatile uint32_t *)(VERIF_CA_TABLE_ADDR + 12))  /* starting minstret */
#define CA_TBL_ENTRY ((volatile verif_ca_entry_t *)(VERIF_CA_TABLE_ADDR + 16))

static uint32_t g_ca_n;

/* Sink that keeps the optimizer from removing things + the load target buffer (.bss, DRAM) */
volatile uint32_t g_ca_sink;
volatile uint32_t g_ca_buf[16];

static void ca_table_init(void)
{
    g_ca_n = 0;
    CA_TBL_COUNT = 0;
    CA_TBL_MAGIC = VERIF_CA_MAGIC;
}

static void ca_record(const char *name, uint32_t cyc, uint32_t ins, uint32_t iter)
{
    volatile verif_ca_entry_t *e;
    int i;

    if (g_ca_n >= VERIF_CA_MAX_ENTRY) return;
    e = &CA_TBL_ENTRY[g_ca_n];
    /* Fixed 8-byte copy. Past the end of name the rest is 0-padded (the host
     * prints it directly with %s) */
    for (i = 0; i < 8; i++) {
        char c = name[i];
        e->name[i] = c;
        if (c == '\0') { for (; i < 8; i++) e->name[i] = 0; break; }
    }
    e->cyc_lo = cyc;
    e->cyc_hi = 0;
    e->ins_lo = ins;
    e->ins_hi = 0;
    e->n_iter = iter;
    e->flags  = 0;
    g_ca_n++;
    CA_TBL_COUNT = g_ca_n;
}

/* -- Measurement blocks --------------------------------------
 * All noinline. The instruction count is pinned down inside the inline asm.
 */

/* 0) Overhead calibration: a block that does nothing */
static void __attribute__((noinline)) blk_empty(void)
{
    __asm__ volatile("" ::: "memory");
}

/* 1) 512 NOPs - no dependencies, no memory. Ideal CPI=1.0 on the 5-stage in-order Rocket */
static void __attribute__((noinline)) blk_nop(void)
{
    __asm__ volatile(
        ".option push\n.option norvc\n"
        ".rept 512\n nop\n .endr\n"
        ".option pop\n" ::: "memory");
}

/* 2) A dependent ALU chain of 512 - the ALU forwards in 1 cycle, so again CPI ~= 1.0 */
static void __attribute__((noinline)) blk_aludep(void)
{
    uint32_t acc = 1;
    __asm__ volatile(
        ".option push\n.option norvc\n"
        ".rept 512\n addi %0, %0, 1\n .endr\n"
        ".option pop\n"
        : "+r"(acc) :: "memory");
    g_ca_sink = acc;
}

/* 3) load-use interlock: the result of lw is used immediately -> 1 stall cycle
 *    every time. Instruction count 512 = (lw+add) x 256 */
static void __attribute__((noinline)) blk_lduse(void)
{
    uint32_t acc = 0;
    volatile uint32_t *p = g_ca_buf;
    __asm__ volatile(
        ".option push\n.option norvc\n"
        ".rept 256\n"
        " lw   t0, 0(%1)\n"
        " add  %0, %0, t0\n"
        ".endr\n"
        ".option pop\n"
        : "+r"(acc) : "r"(p) : "t0", "memory");
    g_ca_sink = acc;
}

/* 4) Control: the same 512 instructions and the same 512 bytes accessed, but
 *    the load result is not used immediately (the instruction after lw is
 *    independent) -> no interlock. The instruction count is exactly the same as
 *    in 3). */
static void __attribute__((noinline)) blk_ldindep(void)
{
    uint32_t acc = 0;
    volatile uint32_t *p = g_ca_buf;
    __asm__ volatile(
        ".option push\n.option norvc\n"
        ".rept 256\n"
        " lw   t0, 0(%1)\n"
        " addi %0, %0, 1\n"
        ".endr\n"
        ".option pop\n"
        : "+r"(acc) : "r"(p) : "t0", "memory");
    g_ca_sink = acc;
}

/* 5) 256 taken branches (addi + bnez = 512 instr). A front-end bubble per taken branch */
static void __attribute__((noinline)) blk_brtaken(void)
{
    uint32_t n = CA_N_BR;
    __asm__ volatile(
        ".option push\n.option norvc\n"
        "1:\n"
        " addi %0, %0, -1\n"
        " bnez %0, 1b\n"
        ".option pop\n"
        : "+r"(n) :: "memory");
    g_ca_sink = n;
}

/* 6) Control: 512 not-taken branches. Same instruction count of 512 as in 5) */
static void __attribute__((noinline)) blk_brnottaken(void)
{
    __asm__ volatile(
        ".option push\n.option norvc\n"
        ".rept 512\n bnez zero, 9f\n .endr\n"
        "9:\n"
        ".option pop\n" ::: "memory");
}

/* 7) 256 muls (mutually independent) - the Rocket MulDiv is multi-cycle, not pipelined */
static void __attribute__((noinline)) blk_mulindep(void)
{
    uint32_t a = 0x12345, b = 0x6789;
    __asm__ volatile(
        ".option push\n.option norvc\n"
        ".rept 256\n mul t0, %0, %1\n .endr\n"
        ".option pop\n"
        :: "r"(a), "r"(b) : "t0", "memory");
}

/* 8) A dependent chain of 256 muls - each result feeds the next mul */
static void __attribute__((noinline)) blk_muldep(void)
{
    uint32_t a = 3, b = 5;
    __asm__ volatile(
        ".option push\n.option norvc\n"
        ".rept 256\n mul %0, %0, %1\n .endr\n"
        ".option pop\n"
        : "+r"(a) : "r"(b) : "memory");
    g_ca_sink = a;
}

/* 9) 64 divs - the Rocket divider is far longer than mul */
static void __attribute__((noinline)) blk_div(void)
{
    uint32_t a = 0x7FFFFFFFu, b = 3;
    __asm__ volatile(
        ".option push\n.option norvc\n"
        ".rept 64\n divu t0, %0, %1\n .endr\n"
        ".option pop\n"
        :: "r"(a), "r"(b) : "t0", "memory");
}

/* 10) 256 csrr mcycle - the cost of CSR access itself */
static void __attribute__((noinline)) blk_csrrd(void)
{
    __asm__ volatile(
        ".option push\n.option norvc\n"
        ".rept 256\n csrr t0, mcycle\n .endr\n"
        ".option pop\n" ::: "t0", "memory");
}

/* 11)/12) Run an identical instruction stream (512 lw + add pairs = 1024
 *   instr) with only the stride changed.
 *   stride=0    -> always the same line = D$ hit             (DHIT)
 *   stride=4096 -> fixed L1 set index + L2 16-set thrash = constant misses
 *                                                            (DMISS)
 * The instruction count, instruction kinds and order are completely identical,
 * so a commit log cannot tell the two blocks apart. The difference shows up
 * only in the real cycle count.
 */
#define CA_WALK_BODY \
        ".option push\n.option norvc\n" \
        ".rept 512\n" \
        " lw  t0, 0(%0)\n" \
        " add %0, %0, %1\n" \
        ".endr\n" \
        ".option pop\n"

static void __attribute__((noinline)) blk_dhit(void)
{
    uint32_t p = (uint32_t)(uintptr_t)g_ca_buf;
    uint32_t stride = 0;
    __asm__ volatile(CA_WALK_BODY : "+r"(p) : "r"(stride) : "t0", "memory");
    g_ca_sink = p;
}

static void __attribute__((noinline)) blk_dmiss(void)
{
    uint32_t p = CA_DMISS_BASE;
    uint32_t stride = CA_DMISS_STRIDE;
    __asm__ volatile(CA_WALK_BODY : "+r"(p) : "r"(stride) : "t0", "memory");
    g_ca_sink = p;
}

/* -- Runner ---------------------------------------------------
 * Calls fn twice: the first run warms up, the second is measured.
 * DMISS creates structural misses via the stride, so it keeps missing even
 * after the warm-up.
 */
static void ca_run(const char *name, void (*fn)(void), uint32_t iter)
{
    uint64_t c0, c1, i0, i1;

    fn();                                   /* warm-up (I$) */

    c0 = verif_ca_get_cycle();
    i0 = verif_ca_get_instret();
    fn();                                   /* measured */
    i1 = verif_ca_get_instret();
    c1 = verif_ca_get_cycle();

    ca_record(name, (uint32_t)(c1 - c0), (uint32_t)(i1 - i0), iter);
}

/* Pre-touch the DMISS working set: avoids reading uninitialized DRAM */
static void ca_dmiss_init(void)
{
    uint32_t i;
    for (i = 0; i < CA_N_DMEM; i++)
        *(volatile uint32_t *)(CA_DMISS_BASE + i * CA_DMISS_STRIDE) = i;
}

int main(void)
{
    ca_table_init();

    /* Proof #1 that the host get_cycle() mirror works (at measurement start).
     * The mirror overwrites the same address, so the starting value is also
     * left in the table header, letting the host compute
     * "interval cycles = mirror(end) - header(start)" itself. */
    verif_ca_mark_cycle();
    CA_TBL_CYC0 = verif_ca_mcycle_lo();
    CA_TBL_INS0 = verif_ca_minstret_lo();

    ca_dmiss_init();

    ca_run("OVERHEAD", blk_empty,       1);
    ca_run("NOP",      blk_nop,         CA_N_NOP);
    ca_run("ALUDEP",   blk_aludep,      CA_N_ALUDEP);
    ca_run("LDUSE",    blk_lduse,       CA_N_LDPAIR);
    ca_run("LDINDEP",  blk_ldindep,     CA_N_LDPAIR);
    ca_run("BRTAKEN",  blk_brtaken,     CA_N_BR);
    ca_run("BRNTAKN",  blk_brnottaken,  CA_N_BRNT);
    ca_run("MULINDP",  blk_mulindep,    CA_N_MUL);
    ca_run("MULDEP",   blk_muldep,      CA_N_MUL);
    ca_run("DIVU",     blk_div,         CA_N_DIV);
    ca_run("CSRRD",    blk_csrrd,       CA_N_CSR);
    ca_run("DHIT",     blk_dhit,        CA_N_DMEM);
    ca_run("DMISS",    blk_dmiss,       CA_N_DMEM);

    /* Proof #2 that the host get_cycle() mirror works (at measurement end).
     * From the difference between this and #1 the host directly observes "the
     * DUT consumed this many cycles". */
    verif_ca_mark_cycle();

    /* Make the table stores visible before the BP */
    __asm__ volatile("fence rw, rw" ::: "memory");
    dut_set_bp(BP_TEST_end);

#ifdef CA_UART
    /* The UART costs tens of seconds per character. Even when enabled, print
     * the bare minimum. */
    {
        uint32_t k;
        uart_init();
        for (k = 0; k < g_ca_n; k++) {
            volatile verif_ca_entry_t *e = &CA_TBL_ENTRY[k];
            printf("%s %u/%u\n", (const char *)e->name, e->cyc_lo, e->ins_lo);
        }
        printf("CADONE\n");
        uart_flush();
    }
#endif

    return CODE_SUCCESS;
}
