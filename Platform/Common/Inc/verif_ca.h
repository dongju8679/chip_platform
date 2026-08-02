#ifndef __VERIF_CA__
#define __VERIF_CA__

/* verif_ca.h - DUT-side CA (cycle-accurate) measurement support
 *              (v301, additive only)
 *
 * Purpose
 *   get_cycle() in the host (verif_host.h) was written on the assumption that
 *   "the DUT mirrors mcycle to a known address", but the DUT had no such
 *   mirroring code. This header fills in that missing piece
 *   (verif_ca_mark_cycle) plus the CA result table contract.
 *
 * Why this is execution-driven only
 *   mcycle counts "how many core clocks elapsed", not "how many instructions
 *   executed". A commit log (ISS trace) contains only the list of committed
 *   instructions - no stalls, bubbles or misses. RTL (Verilator) is
 *   execution-driven cycle-accurate, so the mcycle value itself is the real
 *   cycle count including pipeline stalls. Read together with minstret it gives
 *   CPI = delta(mcycle) / delta(minstret) directly.
 *
 * -- Address map (mailbox region 0x80010000~, DTIM_BASE) -------------
 *   0x80010000  BREAK_POINT        (util.h, existing)
 *   0x80010004  TEST/RESULT        (existing, PLIC_latency etc.)
 *   0x80010008  BREAK_ADDR of I2SR_int_muldiv (exit flag) - IN USE
 *   0x8001000C  (old VERIF_CYCLE_HI - adjacent to 0x08, hence unsafe)
 *   0x80010010  IRQ_INJECT         (existing)
 *   0x80010020  DUT_DBG            (existing)
 *   0x80010040  CA mcycle mirror LO   (new, no collision)
 *   0x80010044  CA mcycle mirror HI   (new)
 *   0x80010048  CA minstret mirror LO (new)
 *   0x8001004C  CA minstret mirror HI (new)
 *   0x80010100  CA result table header/entries (new)
 *   0x80011000  BUF (existing, CLINT/I2SR etc.)
 *   0x80012000  WFI DTIM test (existing)
 *
 *   The old VERIF_CYCLE_LO_ADDR (0x80010008) was the same address as
 *   I2SR_int_muldiv's BREAK_ADDR. The two cases are never run together, but
 *   having a mirror address the host may read at any time overlap another
 *   test's exit flag is an obvious landmine, so it was moved to 0x80010040
 *   (verif_host.h was updated to the same value).
 */

#include <stdint.h>

/* -- Addresses shared with the host (must match verif_host.h exactly) -- */
#include "verif_mbx_base.h"
#define VERIF_CA_CYCLE_LO_ADDR   (VERIF_MBX_BASE + VERIF_MBX_OFF_CYCLE_LO)
#define VERIF_CA_CYCLE_HI_ADDR   (VERIF_MBX_BASE + VERIF_MBX_OFF_CYCLE_HI)
#define VERIF_CA_INSTR_LO_ADDR   (VERIF_MBX_BASE + VERIF_MBX_OFF_INSTR_LO)
#define VERIF_CA_INSTR_HI_ADDR   (VERIF_MBX_BASE + VERIF_MBX_OFF_INSTR_HI)

/* CA result table: [0]=magic [1]=entry count, then an array of 32-byte entries */
#define VERIF_CA_TABLE_ADDR      (VERIF_MBX_BASE + VERIF_MBX_OFF_CA_TABLE)
#define VERIF_CA_MAGIC           0xCA5EC0DEu
#define VERIF_CA_MAX_ENTRY       24

/* Fixed at 32 bytes. The host (verif_host.h) reads the identical layout. */
typedef struct {
    char     name[8];    /* '\0' padded, up to 8 characters */
    uint32_t cyc_lo;     /* delta(mcycle)   (raw, includes measurement overhead) */
    uint32_t cyc_hi;
    uint32_t ins_lo;     /* delta(minstret) */
    uint32_t ins_hi;
    uint32_t n_iter;     /* logical iteration count of the block (for per-iteration values) */
    uint32_t flags;      /* bit0: overhead compensation applied */
} verif_ca_entry_t;

/* -- CSR reads (rv32: a 64-bit counter is read as separate lo/hi halves) --
 * On rv32 the mcycleh high word can roll over between reading it and reading
 * lo, so the canonical form is a hi/lo/hi re-check loop. (These measurements
 * stay within hi=0, but the contract is followed anyway.)
 */
static inline uint32_t verif_ca_mcycle_lo(void)
{
    uint32_t v; __asm__ volatile("csrr %0, mcycle" : "=r"(v)); return v;
}
static inline uint32_t verif_ca_mcycle_hi(void)
{
    uint32_t v; __asm__ volatile("csrr %0, mcycleh" : "=r"(v)); return v;
}
static inline uint32_t verif_ca_minstret_lo(void)
{
    uint32_t v; __asm__ volatile("csrr %0, minstret" : "=r"(v)); return v;
}
static inline uint32_t verif_ca_minstret_hi(void)
{
    uint32_t v; __asm__ volatile("csrr %0, minstreth" : "=r"(v)); return v;
}

static inline uint64_t verif_ca_get_cycle(void)
{
    uint32_t hi, lo, hi2;
    do { hi = verif_ca_mcycle_hi(); lo = verif_ca_mcycle_lo();
         hi2 = verif_ca_mcycle_hi(); } while (hi != hi2);
    return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t verif_ca_get_instret(void)
{
    uint32_t hi, lo, hi2;
    do { hi = verif_ca_minstret_hi(); lo = verif_ca_minstret_lo();
         hi2 = verif_ca_minstret_hi(); } while (hi != hi2);
    return ((uint64_t)hi << 32) | lo;
}

/* -- Update the mirror that the host's get_cycle() reads --
 * verif_host.h::get_cycle() only does master_read(VERIF_CYCLE_LO/HI_ADDR).
 * The host sees the mcycle value as of the moment the DUT called this function.
 * (This is the only path linking host and DUT cycles without DPI.)
 */
static inline void verif_ca_mark_cycle(void)
{
    uint32_t lo = verif_ca_mcycle_lo();
    uint32_t hi = verif_ca_mcycle_hi();
    *(volatile uint32_t *)VERIF_CA_CYCLE_HI_ADDR = hi;   /* hi first, so lo is the freshest */
    *(volatile uint32_t *)VERIF_CA_CYCLE_LO_ADDR = lo;
    *(volatile uint32_t *)VERIF_CA_INSTR_HI_ADDR = verif_ca_minstret_hi();
    *(volatile uint32_t *)VERIF_CA_INSTR_LO_ADDR = verif_ca_minstret_lo();
}

#endif /* __VERIF_CA__ */
