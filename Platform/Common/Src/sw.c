/* sw.c - CLINT MSIP handler registration (shared HAL)
 *
 * Provenance: promoted verbatim from
 *       verif/ip/S5740/CLINT/sw_interrupt/Src/sw.c. The body is byte-identical
 *       to the rt-dev original (only this comment block was added).
 *       See Inc/sw.h for the design rationale (why it was not merged into
 *       interrupt.c).
 *
 * Override: if the test folder (verif/.../Src) contains a sw.c of the same
 *       name, Makefile.VERIF drops this file from the link and uses the test's
 *       copy. verif/ip/S5740/CLINT/sw_interrupt does carry its own sw.c, so
 *       that path is always exercised.
 *       (See chip_docs/verif/docs/baremetal.md)
 */
#include "sw.h"
#include "interrupt.h"

void init_swint_handler(void)
{
    pISR_BASE.sw_int_handler = empty;
    SET_INTERRUPT_ENABLE(MSIE);
}

int register_swint_handler(int (*handler)(void))
{
    pISR_BASE.sw_int_handler = handler;
    return 0;
}
