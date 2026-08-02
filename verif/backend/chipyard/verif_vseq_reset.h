/* verif_vseq_reset.h - * intentionally has NO include guard. Include it repeatedly.
 *
 * verif_host.h pulls several company Vseq .cc files into one translation unit, and each of them
 * includes its own Inc/<test>.h. Those headers all use the same macro names with per-test values:
 *
 *     CLINT_sw_interrupt.h    BP_MAX 5    TEST_DEBUG_BASE_ADDR 0x80011000
 *     CLINT_timer_interrupt.h BP_MAX 50   TEST_DEBUG_BASE_ADDR 0x80011000
 *     PLIC_enable.h           BP_MAX 1    TEST_DEBUG_BASE_ADDR 0x80011000  MAX_PLIC_NUMBER 45
 *     I2SR_int_muldiv.h       BP_MAX 1    TEST_DEBUG_BASE_ADDR 0x80010004
 *
 * Each .cc uses its macros immediately after its own header, so the include sequence is already
 * self-consistent and every test sees its own values. Dropping the previous definitions here just
 * keeps the build free of "macro redefined" warnings, and makes the fact that these names are
 * per-test - not global - explicit.
 */
#undef BP_MAX
#undef TEST
#undef TEST_ADDR
#undef TEST_DEBUG_BASE_ADDR
#undef MAX_PLIC_NUMBER
