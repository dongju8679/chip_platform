/* S5740_test.h  -  stage 1 TinyRocket bring-up SHIM
 *
 * Purpose: without the real S5740_test.h from Verification_SDK, provide a minimal API
 *       to run "firmware self-checking" VERIF tests like CLINT/rtc on TinyRocket + Verilator
 *       and report PASS/FAIL via tohost.
 *
 * When the real S5740_test.h is available: replace this file with it, and
 * just wire the inpdw/outpdw/exit bodies to the hal_shim.c implementation.
 * (symbols this header defines: inpdw, outpdw, exit, CODE_SUCCESS, CODE_FAIL, types)
 *
 * What CLINT_rtc.c actually uses: inpdw(), exit(), CODE_SUCCESS, CODE_FAIL,
 *   uint64_t, true, and the typo type unit32_t.
 */
#ifndef S5740_TEST_H_STAGE1_SHIM
#define S5740_TEST_H_STAGE1_SHIM

#include <stdint.h>
#include <stdbool.h>

/* Accept the original test source typo (unit32_t) so it still compiles.
 * If the real S5740_test.h does not define unit32_t, it is a test-side typo,
 * verify it together when the original is available. */
typedef uint32_t unit32_t;

/* Exit codes.
 * With CODE_SUCCESS=0, when main returns CODE_SUCCESS
 * the standard riscv-tests convention (crt: tohost=(ret<<1)|1) yields tohost=1 (PASS).
 * Even if it differs from the real value, in stage 1 this header is the single source, so it is fine. */
#define CODE_SUCCESS   0
#define CODE_FAIL      1

/* MMIO access (in stage 1, handled by the software mock in hal_shim.c) */
uint32_t inpdw(uint32_t addr);
void     outpdw(uint32_t addr, uint32_t val);

/* exit -> tohost (hal_shim.c) */
void     exit(int code) __attribute__((noreturn));


/* -- rt-dev test API (dut side BP handshake) --
 * rt-dev's S5740_test.h provides these; added here for the ported test case.
 *   dut_set_bp(n)  : DUT -> host progress signal (writes BREAK_ADDR)
 *   dut_wait_bp(n) : DUT waits until BREAK_ADDR == n
 * BREAK_ADDR is 0x80010000 (chipyard mailbox; host_set_bp writes the same address). */
#define VERIF_DUT_BREAK_ADDR 0x80010000u
static inline void dut_set_bp(unsigned int bp)  { outpdw(VERIF_DUT_BREAK_ADDR, bp); }
static inline void dut_wait_bp(unsigned int bp) { while (inpdw(VERIF_DUT_BREAK_ADDR) != bp) { } }

#endif /* S5740_TEST_H_STAGE1_SHIM */
