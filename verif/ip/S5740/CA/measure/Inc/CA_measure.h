#ifndef __CA_MEASURE__
#define __CA_MEASURE__

/* CA_measure.h - execution-driven cycle-accurate cycle measurement case (v301)
 *
 * What this case proves
 *   "The value you cannot get from a commit log (ISS trace), only from
 *   execution-driven RTL" = the real cycle count including pipeline stalls.
 *
 *   A commit log is a list of committed instructions -> it gives the
 *   instruction count (minstret). But load-use interlocks, taken-branch
 *   bubbles, multi-cycle mul/div and cache misses leave no trace in a commit
 *   log. For the same instruction count the real cycle count can differ by
 *   several times.
 *   This case deliberately includes contrast pairs that have "the same
 *   instruction count but different cycles" and shows the difference
 *   numerically.
 *
 *     LDUSE  vs LDINDEP   : both 512 instr, only the load-use distance differs
 *     BRTAKEN vs BRNTAKEN : both 512 instr, only taken vs not-taken differs
 *     DHIT   vs DMISS     : both 512 instr, only the working set size differs
 *     MULINDEP vs MULDEP  : both 256 instr, only the mul dependency differs
 *
 * Host interaction
 *   The DUT writes its results into the table at 0x80010100 and sets
 *   BP_TEST_end. The host (verif_host.h, +verif=CA_measure) master_reads it and
 *   prints a table.
 *   The UART costs tens of seconds per character, so it is OFF by default
 *   (enabled only with -DCA_UART).
 */

#include "verif_ca.h"

/* Logical iteration count of each block (used to compute per-iteration cycles) */
#define CA_N_NOP      512
#define CA_N_ALUDEP   512
#define CA_N_LDPAIR   256   /* lw + 1 instr = 512 instr */
#define CA_N_BR       256   /* addi + branch = 512 instr */
#define CA_N_BRNT     512
#define CA_N_MUL      256
#define CA_N_DIV      64
#define CA_N_CSR      256
#define CA_N_DMEM     512

/* DMISS working set: DRAM that does not overlap code or the mailbox
 * (0x80000000~0x8001FFFF).
 * DTS: memory@80000000 size 0x10000000 (256MB), so 0x80100000 is safe.
 * stride 4096 -> the L1 D$ (32K/64 sets) set index is always the same, which
 * exceeds 8-way and misses structurally; L2 (512K/1024 sets) also uses only
 * 16 sets and thrashes.
 */
#define CA_DMISS_BASE   0x80100000u
#define CA_DMISS_STRIDE 4096u

#endif /* __CA_MEASURE__ */
