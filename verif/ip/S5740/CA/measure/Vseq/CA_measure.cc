/* CA_measure.cc - host Vseq: execution-driven cycle-accurate measurement readout.
 *
 * Unlike its neighbours in verif/ip, this sequence has no company original - CA_measure is our
 * own case. It lives here anyway so that every co-sim test is found in the same place and
 * verif_host.h stays pure dispatch. (Moved out of verif_host.h::run_vseq in the v304 refactor.)
 *
 * What it does
 *   The DUT (verif/ip/S5740/CA/measure/Src) times reproducible code blocks with mcycle/minstret
 *   and leaves a table at VERIF_CA_TABLE_ADDR. The host only reads it out - no UART, because a
 *   single UART character costs tens of seconds of wall clock on the RTL simulator.
 *
 * Why this needs an execution-driven (RTL) backend
 *   mcycle counts core clocks, so it includes load-use interlocks, taken-branch bubbles,
 *   multi-cycle mul/div and cache misses. A commit log only records retired instructions
 *   (= minstret), so it can never produce the cycle column.
 *
 * Note it includes only S5740_mcu_tests.h, not CA_measure.h: that header is the DUT's
 * (it pulls in Platform/Common/Inc/verif_ca.h, which is not on the host include path).
 * The table layout the host needs is in verif_addr_chipyard.h, which S5740_mcu_tests.h provides.
 */
#include "S5740_mcu_tests.h"
#include <string>
#include <vector>

bool CA_measure()
{
    fprintf(stderr, "[verif] CA_measure: waiting for DUT (bp=%u)\n", BP_TEST_end);
    if (host_wait_bp(BP_TEST_end)) {
        fprintf(stderr, "[verif] CA_measure: DUT never reached BP_TEST_end\n");
        return false;
    }
    /* host_get_cycle()/host_get_instret(): value of the DUT mcycle mirror at the moment the DUT
     * last called verif_ca_mark_cycle() (end of the measurement run). */
    uint64_t cyc_end = host_get_cycle(), ins_end = host_get_instret();

    unsigned int magic = 0, count = 0;
    if (!master_read(VERIF_CA_TABLE_ADDR + 0, magic)) return false;
    if (!master_read(VERIF_CA_TABLE_ADDR + 4, count)) return false;
    if (magic != VERIF_CA_MAGIC || count == 0 || count > 24) {
        fprintf(stderr, "[verif] CA_measure: bad result table (magic=0x%08x count=%u)\n", magic, count);
        return false;
    }

    /* Read the whole table first, then report. Two views are printed:
     *   RAW  = exactly what mcycle/minstret said across the block
     *   NET  = RAW minus the OVERHEAD block (empty block: 4x csrr + indirect call).
     * NET is what you compare between blocks; RAW is what is actually measured.
     * NOTE the NET CPI of a pure-ALU block lands slightly under 1.0. That is not a sub-1-CPI core
     * (single-issue in-order can't do that) - the OVERHEAD block's csrr instructions are themselves
     * multi-cycle (see CSRRD, ~2.9 cyc each), so subtracting it over-corrects by a few percent.
     * The overhead-free numbers are the CONTROLLED PAIRS printed below, which are differences of
     * two blocks measured with the exact same instrumentation. */
    struct ca_row { char name[9]; unsigned int cyc, ins, iter, ncyc, nins; };
    std::vector<ca_row> rows;
    unsigned int ov_cyc = 0, ov_ins = 0;
    fprintf(stderr,
      "[verif] ---- CA_measure : execution-driven cycle-accurate (RV32RocketConfig) ----\n");
    fprintf(stderr,
      "[verif] %-9s %9s %9s %9s %9s %8s %10s\n",
      "block", "cyc_raw", "ins_raw", "cyc_net", "ins_net", "CPI", "cyc/iter");
    for (unsigned int i = 0; i < count; i++) {
        unsigned int base = VERIF_CA_TABLE_ADDR + 16 + i * VERIF_CA_ENTRY_SZ;
        unsigned int w0 = 0, w1 = 0, cyc = 0, ins = 0, iter = 0, dummy = 0;
        master_read(base + 0,  w0);
        master_read(base + 4,  w1);
        master_read(base + 8,  cyc);
        master_read(base + 12, dummy);           /* cyc_hi (0 in this range) */
        master_read(base + 16, ins);
        master_read(base + 20, dummy);           /* ins_hi */
        master_read(base + 24, iter);
        ca_row r;
        for (int b = 0; b < 4; b++) r.name[b]     = (char)((w0 >> (8 * b)) & 0xFF);
        for (int b = 0; b < 4; b++) r.name[4 + b] = (char)((w1 >> (8 * b)) & 0xFF);
        r.name[8] = '\0';
        r.cyc = cyc; r.ins = ins; r.iter = iter;

        if (i == 0) { ov_cyc = cyc; ov_ins = ins; }   /* first entry is OVERHEAD */
        r.ncyc = (cyc > ov_cyc) ? (cyc - ov_cyc) : 0;
        r.nins = (ins > ov_ins) ? (ins - ov_ins) : 0;
        rows.push_back(r);

        if (i == 0)
            fprintf(stderr, "[verif] %-9s %9u %9u %9s %9s %8s %10s  (calibration)\n",
                    r.name, cyc, ins, "-", "-", "-", "-");
        else
            fprintf(stderr, "[verif] %-9s %9u %9u %9u %9u %8.3f %10.2f\n",
                    r.name, cyc, ins, r.ncyc, r.nins,
                    r.nins ? (double)r.ncyc / (double)r.nins : 0.0,
                    r.iter ? (double)r.ncyc / (double)r.iter : 0.0);
    }
    fprintf(stderr, "[verif] ------------------------------------------------------------\n");

    /* -- controlled pairs: the actual "commit-log can't see this" evidence --
     * Each pair runs the SAME instruction sequence with the SAME instrumentation. Their
     * instruction counts are equal (or within the loop prologue), so a commit log cannot tell
     * them apart. The cycle difference is the stall, isolated. */
    struct { const char* a; const char* b; unsigned int n; const char* what; } pairs[] = {
        {"LDUSE",   "LDINDEP", 256, "load-use interlock   (per lw whose result is used next)"},
        {"BRTAKEN", "BRNTAKN", 256, "taken-branch bubble  (per taken branch)"},
        {"DMISS",   "DHIT",    512, "cache miss           (per L1+L2-missing lw)"},
    };
    fprintf(stderr, "[verif] CONTROLLED PAIRS (same instructions, cycle delta = pure stall)\n");
    fprintf(stderr, "[verif] %-9s %-9s %8s %8s %9s  %s\n",
            "block_A", "block_B", "dInstr", "dCycle", "cyc/event", "isolates");
    for (unsigned int p = 0; p < sizeof(pairs) / sizeof(pairs[0]); p++) {
        const ca_row *A = 0, *B = 0;
        for (size_t k = 0; k < rows.size(); k++) {
            if (std::string(rows[k].name) == pairs[p].a) A = &rows[k];
            if (std::string(rows[k].name) == pairs[p].b) B = &rows[k];
        }
        if (!A || !B) continue;
        long dcyc = (long)A->ncyc - (long)B->ncyc;
        long dins = (long)A->nins - (long)B->nins;
        fprintf(stderr, "[verif] %-9s %-9s %8ld %8ld %9.2f  %s\n",
                pairs[p].a, pairs[p].b, dins, dcyc,
                (double)dcyc / (double)pairs[p].n, pairs[p].what);
    }
    fprintf(stderr, "[verif] ------------------------------------------------------------\n");

    unsigned int cyc0 = 0, ins0 = 0;
    master_read(VERIF_CA_TABLE_ADDR + 8,  cyc0);
    master_read(VERIF_CA_TABLE_ADDR + 12, ins0);
    fprintf(stderr, "[verif] host get_cycle()  = %llu  (DUT mcycle mirror @0x%08x)\n",
            (unsigned long long)cyc_end, VERIF_CYCLE_LO_ADDR);
    fprintf(stderr, "[verif] host get_instret()= %llu  (DUT minstret mirror @0x%08x)\n",
            (unsigned long long)ins_end, VERIF_INSTR_LO_ADDR);
    fprintf(stderr, "[verif] whole-run via mirror: %llu cycles / %llu instr (CPI %.3f)\n",
            (unsigned long long)(cyc_end - cyc0),
            (unsigned long long)(ins_end - ins0),
            (ins_end > ins0) ? (double)(cyc_end - cyc0) / (double)(ins_end - ins0) : 0.0);
    fprintf(stderr, "[verif] supports_ca()=%d, blocks=%u\n", (int)host_supports_ca(), count);
    return true;
}

bool CA_measure_check()
{
    return true;
}
