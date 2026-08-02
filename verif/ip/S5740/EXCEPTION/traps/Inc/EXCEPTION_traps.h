#ifndef __EXCEPTION_TRAPS__
#define __EXCEPTION_TRAPS__

/* EXCEPTION_traps.h - exception trap verification case definitions
 *
 * Each sub-test is assigned one bit. On failure that bit is set, and at the end
 * the failure mask is printed to the HTIF console before exit(CODE_FAIL).
 * Why a bitmask: so a single run reveals every exception that failed to fire.
 * (Dying on the first failure would mean re-running for minutes each time.)
 */

#define EXC_T_BOOT        (1u << 0)   /* boot_check(): sp/gp/mtvec/.bss */
#define EXC_T_ILLEGAL     (1u << 1)   /* mcause 2  : illegal instruction */
#define EXC_T_BREAKPOINT  (1u << 2)   /* mcause 3  : ebreak */
#define EXC_T_ECALL       (1u << 3)   /* mcause 11 : ecall from M-mode */
#define EXC_T_LOAD_MISAL  (1u << 4)   /* mcause 4  : misaligned load */
#define EXC_T_STORE_MISAL (1u << 5)   /* mcause 6  : misaligned store */
#define EXC_T_EPC         (1u << 6)   /* is the recorded mepc the trapping instruction address */
#define EXC_T_COUNT       (1u << 7)   /* does the cumulative exception count match */
#define EXC_T_RESUME      (1u << 8)   /* does execution resume normally (SKIP) after an exception */

#define EXC_PASS_MARK "EXCEPTION_TRAPS_PASS"
#define EXC_FAIL_MARK "EXCEPTION_TRAPS_FAIL "

#endif
