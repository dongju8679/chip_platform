/* PMP_violation.c - RV32RocketConfig PMP verification (self-check)
 *
 * -- What this proves -------------------------------------------------
 *  1. The PMP address encoding (NA4/NAPOT) follows the spec.
 *  2. IMPORTANT: in M-mode the check only applies when the L(lock) bit is set.
 *     This is the trap almost everyone hits when first using PMP, so the test
 *     deliberately contrasts "passes before locking -> traps after locking".
 *  3. On a violation, mcause 5 (load) / 7 (store) and mtval (the violating
 *     address) are correct.
 *  4. A lock is irreversible (attempts to release it are ignored).
 *  5. Unmatched addresses are still accessible (M-mode default allow).
 *
 * -- DUT specification -------------------------------------------------
 *  dts: riscv,pmpregions = <8>, riscv,pmpgranularity = <4>
 *
 * -- Warning about ordering --------------------------------------------
 *  The moment PMP_TEST_BASE is locked, that region is unusable until reset.
 *  So every check that must happen before locking (the first part of 2) is
 *  completed first.
 *  Never lock the code/stack region (0x80000000..0x8000EFFF) - doing so kills
 *  fetch/stack access on the spot.
 *
 * -- Running ------------------------------------------------------------
 *   ./verif/ip/S5740/PMP/violation/run.sh
 */
#include "type.h"
#include "util.h"
#include "trap.h"
#include "exceptions.h"
#include "pmp.h"
#include "htif.h"
#include "PMP_violation.h"

static unsigned fail_mask;

static void check(unsigned bit, int ok)
{
    if (!ok) fail_mask |= bit;
}

static volatile unsigned int sink;

/* Keep the triggers noinline so the mepc/mtval checks stay stable. */
static void __attribute__((noinline)) do_store_violation(void)
{
    *(volatile unsigned int *)PMP_TEST_BASE = 0xDEADBEEFu;
}

static void __attribute__((noinline)) do_load_violation(void)
{
    sink = *(volatile unsigned int *)PMP_TEST_BASE;
}

int main(void)
{
    unsigned cfg = 0;
    xlen_t   addr = 0;
    xlen_t   expect_addr;
    unsigned before;

    exceptions_init();

    /* -- 1) Encoding self-check (pure computation, no hardware) ---------
     * NA4  : pmpaddr = base >> 2
     * NAPOT: pmpaddr = (base >> 2) | (size/8 - 1)
     *   For 4KB (0x1000) the low bit pattern is 0x1FF (= 0x1000/8 - 1)
     */
    check(PMP_T_ENCODE,
          pmp_napot_encode(0x80020000u, 4u)      == (0x80020000u >> 2) &&
          pmp_napot_encode(0x80020000u, 0x1000u) == ((0x80020000u >> 2) | 0x1FFu) &&
          pmp_napot_encode(0x80020000u, 8u)      == ((0x80020000u >> 2) | 0x0u));

    /* -- 2) Argument checking ------------------------------------------
     * unaligned / not a power of two / index out of range / bad permission bits
     */
    check(PMP_T_ARGCHECK,
          pmp_napot_encode(0x80020004u, 0x1000u) == 0 &&   /* unaligned */
          pmp_napot_encode(0x80020000u, 0x0C00u) == 0 &&   /* not 2^k */
          pmp_set_region(PMP_NUM_REGIONS, PMP_TEST_BASE, PMP_TEST_SIZE,
                         PMP_PERM_RW) < 0 &&               /* index out of range */
          pmp_set_region(PMP_TEST_IDX, PMP_TEST_BASE, PMP_TEST_SIZE,
                         0x40u) < 0);                      /* bad permission bits */

    /* -- 3) Unlocked configuration: did the values reach the CSRs, and does
     *    M-mode still pass? --------------------------------------------
     * The region is configured with all of R/W removed (PERM_NONE). Because
     * L=0, M-mode access must still succeed. This is the core PMP pitfall.
     */
    check(PMP_T_READBACK, pmp_set_region(PMP_TEST_IDX, PMP_TEST_BASE,
                                         PMP_TEST_SIZE, PMP_PERM_NONE) == 0);
    expect_addr = pmp_napot_encode(PMP_TEST_BASE, PMP_TEST_SIZE);
    pmp_get_region(PMP_TEST_IDX, &cfg, &addr);
    check(PMP_T_READBACK,
          addr == expect_addr &&
          (cfg & 0x18u) == PMP_MODE_NAPOT &&
          (cfg & 0x07u) == PMP_PERM_NONE &&
          (cfg & PMP_LOCK) == 0);

    /* With L=0 there is no effect on M-mode access -> no exception may occur */
    before = exception_count();
    *(volatile unsigned int *)PMP_TEST_BASE = 0x11223344u;
    sink = *(volatile unsigned int *)PMP_TEST_BASE;
    check(PMP_T_UNLOCKED,
          exception_count() == before && sink == 0x11223344u);

    /* -- 4) Lock it. Irreversible from this point on -------------------- */
    check(PMP_T_READBACK, pmp_lock_region(PMP_TEST_IDX, PMP_TEST_BASE,
                                          PMP_TEST_SIZE, PMP_PERM_NONE) == 0);
    pmp_get_region(PMP_TEST_IDX, &cfg, &addr);
    check(PMP_T_READBACK, (cfg & PMP_LOCK) != 0 && addr == expect_addr);

    /* -- 5) store violation -> mcause 7 --------------------------------- */
    before = exception_count();
    exception_expect(CAUSE_STORE_ACCESS);
    do_store_violation();
    check(PMP_T_STORE_FAULT,
          exception_matched() &&
          exception_count() == before + 1u &&
          exception_last()->cause == CAUSE_STORE_ACCESS);

    /* mtval must be the violating address */
    check(PMP_T_MTVAL, exception_last()->tval == PMP_TEST_BASE);

    /* -- 6) load violation -> mcause 5 ---------------------------------- */
    before = exception_count();
    exception_expect(CAUSE_LOAD_ACCESS);
    do_load_violation();
    check(PMP_T_LOAD_FAULT,
          exception_matched() &&
          exception_count() == before + 1u &&
          exception_last()->cause == CAUSE_LOAD_ACCESS);

    /* -- 7) The lock is sticky: release attempts must be ignored --------- */
    pmp_clear_region(PMP_TEST_IDX);
    pmp_get_region(PMP_TEST_IDX, &cfg, &addr);
    check(PMP_T_LOCK_STICKY,
          (cfg & PMP_LOCK) != 0 &&
          (cfg & 0x18u) == PMP_MODE_NAPOT &&
          addr == expect_addr);

    /* Since it was not released, the violation must still fire */
    before = exception_count();
    exception_expect(CAUSE_STORE_ACCESS);
    do_store_violation();
    check(PMP_T_LOCK_STICKY,
          exception_matched() && exception_count() == before + 1u);

    /* -- 8) Unmatched addresses remain accessible (M-mode default allow) - */
    before = exception_count();
    *(volatile unsigned int *)PMP_FREE_ADDR = 0x5A5A5A5Au;
    sink = *(volatile unsigned int *)PMP_FREE_ADDR;
    check(PMP_T_NOMATCH_OK,
          exception_count() == before && sink == 0x5A5A5A5Au);

    /* -- Verdict -- */
    if (fail_mask == 0) {
        htif_puts(PMP_PASS_MARK "\n");
        return CODE_SUCCESS;
    }

    htif_puts(PMP_FAIL_MARK);
    htif_puthex(fail_mask);
    htif_puts(" cfg=");
    htif_puthex(cfg);
    htif_puts(" addr=");
    htif_puthex(addr);
    htif_puts(" last_cause=");
    htif_puthex(exception_last()->cause);
    htif_puts("\n");
    return CODE_FAIL;
}
