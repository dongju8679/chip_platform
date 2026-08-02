#ifndef __PMP_VIOLATION__
#define __PMP_VIOLATION__

/* PMP_violation.h - PMP violation trap verification case definitions
 *
 * Same approach as EXCEPTION/traps: assign one bit per sub-test and print the
 * failure mask in one go.
 */

#define PMP_T_ENCODE      (1u << 0)  /* does NAPOT/NA4 address encoding follow the spec */
#define PMP_T_ARGCHECK    (1u << 1)  /* are invalid arguments rejected */
#define PMP_T_READBACK    (1u << 2)  /* did the configured values actually reach the CSRs */
#define PMP_T_UNLOCKED    (1u << 3)  /* with L=0, is M-mode access left unblocked */
#define PMP_T_STORE_FAULT (1u << 4)  /* store after lock -> mcause 7 */
#define PMP_T_LOAD_FAULT  (1u << 5)  /* load after lock  -> mcause 5 */
#define PMP_T_MTVAL       (1u << 6)  /* is mtval the violating address */
#define PMP_T_LOCK_STICKY (1u << 7)  /* is a locked entry impossible to release */
#define PMP_T_NOMATCH_OK  (1u << 8)  /* are unmatched addresses still accessible */

/* The region used for the test.
 *   link.ld ram = 0x80000000 + 60K -> 0x8000F000 (stack top)
 *   mailbox     = 0x80010000, BUF = 0x80011000
 *   DRAM        = 0x80000000 + 256MB (dts memory@80000000, 0x10000000)
 * 0x80020000 is valid DRAM that overlaps neither code, stack nor mailbox.
 * It is locked as a 4KB NAPOT region.
 */
#define PMP_TEST_BASE  0x80020000u
#define PMP_TEST_SIZE  0x1000u
#define PMP_TEST_IDX   0u          /* lower index = higher priority */

/* Control address that is not matched (= untouched by PMP) */
#define PMP_FREE_ADDR  0x80030000u

#define PMP_PASS_MARK "PMP_VIOLATION_PASS"
#define PMP_FAIL_MARK "PMP_VIOLATION_FAIL "

#endif
