#ifndef __EXCEPTIONS__
#define __EXCEPTIONS__

/* exceptions.h - shared exception handling layer (RV32, M-mode)
 *
 * -- How exceptions were handled before ------------------------------
 *  - Platform/Common/Src/syscalls.c : trap_handler() unconditionally called
 *      exit(CODE_FATAL) for any exception (mcause bit31=0). In other words,
 *      "exception = immediate death".
 *      Only trap_handler_PMP() was an exception: it counted mcause 1/5/7 and
 *      moved on, but fixed up the resume address by hardcoding a frame offset
 *      as asm("lw ra,12(sp)") (see the trap.h comment - that code can break
 *      depending on the optimization level).
 *  - Platform/Chipset/S5740/Src/exceptions.c (50 lines) records the exception
 *      into an EXIT_INFO struct, notifies the master over IPC and then dies.
 *      flush_ipc(), alert_master(), GPIO_IRQ_ADDR and pEXIT_INFO are all
 *      S5740-specific hardware and cannot be brought up on chipyard, so that
 *      file itself was not made common.
 *      This layer takes only the "record the exception and die" policy and
 *      swaps the recording medium from IPC to an in-memory log.
 *
 * -- So what this API does -------------------------------------------
 *  "Make exceptions observable."
 *  From a verification standpoint three things are needed:
 *    (1) which exception occurred, how many times, at which address
 *        -> exception_record_t
 *    (2) whether the test can continue after the exception
 *        -> per-cause policy (EXC_ACT_x)
 *    (3) the test decides whether the exception was the expected one
 *        -> exception_expect()
 *  (3) is the crucial one. An exception test is not "fail if an exception
 *  occurs" but "pass only if exactly the expected exception occurs", so the
 *  shape must be: register the expectation up front, then compare against what
 *  actually happened.
 *
 * -- Is this verifiable on chipyard? ---------------------------------
 *  Yes. All of the following actually fire on RV32RocketConfig (rv32imac,
 *  M-mode):
 *    illegal instruction (2), breakpoint (3, ebreak), machine ecall (11),
 *    misaligned load/store (4/6), load/store access fault (5/7, PMP violation)
 *  verif/ip/S5740/EXCEPTION/traps and verif/ip/S5740/PMP/violation use this
 *  layer.
 *
 * -- Usage -----------------------------------------------------------
 *   exceptions_init();                          // mtvec -> _trap_entry_common
 *   exception_expect(CAUSE_BREAKPOINT);         // register the next expectation
 *   __asm__ volatile ("ebreak");
 *   if (!exception_matched()) fail();
 *
 * -- Constraints -----------------------------------------------------
 *  - No nested traps (trap_entry.S does not re-enable mstatus.MIE).
 *  - TRACK=baremetal only. The interrupt path uses PLIC_CLAIM/PLIC_CLEAR
 *    (S5740.h) and pISR_BASE (interrupt.h), both of which Makefile.VERIF
 *    supplies via -include when TRACK=baremetal. They do not link under
 *    TRACK=selfcheck.
 *  - exceptions_init() takes over mtvec entirely. To use interrupts alongside
 *    it, nothing special is needed: trap_dispatch() forwards interrupts to
 *    pISR_BASE (the same dispatch structure as the rt-dev trap_handler() was
 *    kept).
 */

#include "type.h"
#include "trap.h"

/* Handling policy for a single exception */
typedef enum {
    EXC_ACT_FATAL = 0,  /* default: record, then exit(CODE_FATAL) - never pass silently */
    EXC_ACT_SKIP,       /* record, then resume past the trapping instruction (mepc += 2 or 4) */
    EXC_ACT_HANDLER     /* delegate to the registered user handler */
} exc_action_t;

/* User handler. It may modify the frame directly (including mepc).
 * Return 0 = handled (resume); non-zero = exit(CODE_FATAL). */
typedef int (*exc_handler_t)(xlen_t cause, trap_frame_t *frame);

/* Record of the most recent exception */
typedef struct _exception_record {
    xlen_t   cause;     /* low mcause code */
    xlen_t   epc;       /* address of the trapping instruction */
    xlen_t   tval;      /* mtval (faulting address or instruction bits) */
    unsigned count;     /* cumulative exception count since boot */
} exception_record_t;

/* Point mtvec at _trap_entry_common and reset internal state.
 * The default policy for every cause is EXC_ACT_FATAL. */
void exceptions_init(void);

/* Set the per-cause policy. cause is CAUSE_x (the low mcause code). */
void exception_set_action(xlen_t cause, exc_action_t action);

/* Register a per-cause user handler. The policy becomes EXC_ACT_HANDLER
 * automatically. */
void exception_set_handler(xlen_t cause, exc_handler_t handler);

/* Register "this exception is expected next".
 * Once registered, that cause is handled as SKIP regardless of policy and
 * matched is set. If a different exception occurs, matched stays clear and the
 * original policy applies. */
void exception_expect(xlen_t cause);

/* Whether the expected exception actually occurred. Reading it automatically
 * clears the expectation and the result. */
int  exception_matched(void);

/* Last exception record / cumulative count */
const exception_record_t *exception_last(void);
unsigned exception_count(void);

/* Cause code -> human-readable name (for logging). "unknown" if unrecognized. */
const char *exception_name(xlen_t cause);

/* C entry point called by trap_entry.S. Never called directly. */
void trap_dispatch(xlen_t mcause, trap_frame_t *frame);

#endif /* __EXCEPTIONS__ */
