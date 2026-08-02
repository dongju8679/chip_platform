#ifndef __SW__
#define __SW__

/* sw.h - CLINT MSIP (machine software interrupt) handler registration
 *        (shared HAL)
 *
 * -- Provenance ------------------------------------------------------
 * Originally lived in verif/ip/S5740/CLINT/sw_interrupt/Inc/sw.h.
 * Promoted to Platform/Common for the same reason as timer.h.
 * The declarations are the rt-dev original.
 *
 * -- Why this is a separate file instead of being folded into interrupt.c --
 * This project's policy is "copy rt-dev tests in whole folders, unchanged".
 * An rt-dev test folder brings its own sw.c. If the sw functions were merged
 * into interrupt.c, the copied test's sw.c and interrupt.c would define the
 * same symbols twice and the link would break.
 * Keeping the file name identical to rt-dev (sw.c) is what lets the Makefile's
 * "a same-named .c in the test folder wins" rule resolve the conflict
 * automatically.
 * It is therefore kept separate from interrupt.c on purpose.
 *
 * -- Is this verifiable on chipyard? ---------------------------------
 * Yes. CLINT MSIP really exists at 0x02000000, and the host (Vseq) raises the
 * IRQ with master_write(0x02000000,1).
 * verif/ip/S5740/CLINT/sw_interrupt PASSes with this driver.
 *
 * -- Usage -----------------------------------------------------------
 *   init_swint_handler();               // mie.MSIE on, handler = empty
 *   register_swint_handler(my_isr);     // register the real handler
 *   GLOBAL_INTERRUPT_ENABLE();          // mstatus.MIE on
 * The handler must clear the IRQ by writing 0 to MSIP
 * (otherwise it re-enters the moment the trap returns):
 *   outpdw(CLINT_SW_INTERRUPT, 0);
 */

/* Enable mie.MSIE and initialize the sw handler to empty. */
void init_swint_handler(void);

/* Replace the sw interrupt handler. Always returns 0. */
int register_swint_handler(int (*)(void));

#endif
