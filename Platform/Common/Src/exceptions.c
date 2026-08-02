/* exceptions.c - shared exception handling layer (RV32, M-mode)
 *
 * See Platform/Common/Inc/exceptions.h for the design background and API
 * rationale. The trap frame contract lives in Platform/Common/Inc/trap.h and
 * the assembly entry point in Platform/Common/Src/trap_entry.S.
 *
 * -- Why a new layer instead of fixing the rt-dev trap_handler() -----
 * trap_handler() in syscalls.c is the path every existing feature goes through.
 * Touching it would change the CLINT/PLIC/FreeRTOS/benchmark images and force
 * the whole regression to be re-verified. The risk outweighs the benefit.
 * So the interrupt path's behaviour is reproduced exactly (the interrupt
 * dispatch in trap_dispatch below has the same shape as trap_handler()), while
 * only the exception path gets a properly built separate entry point that is
 * used opt-in.
 */
#include "exceptions.h"
#include "interrupt.h"
#include "util.h"

/* -- State -----------------------------------------------------------
 * All in .bss. crt.S zeroes .bss, so EXC_ACT_FATAL(=0) is the default even
 * without explicit initialization. exceptions_init() does not rely on that
 * assumption and initializes explicitly again (so it can be re-initialized).
 */
static unsigned char   exc_action[CAUSE_MAX];
static exc_handler_t   exc_handler[CAUSE_MAX];
static exception_record_t exc_last;
static unsigned        exc_total;

/* Expectation: when expect_valid is set, compare against expect_cause. */
static int    expect_valid;
static xlen_t expect_cause;
static int    expect_hit;

extern void _trap_entry_common(void);   /* trap_entry.S */

void exceptions_init(void)
{
    unsigned i;
    for (i = 0; i < CAUSE_MAX; i++) {
        exc_action[i]  = (unsigned char)EXC_ACT_FATAL;
        exc_handler[i] = 0;
    }
    exc_last.cause = 0; exc_last.epc = 0; exc_last.tval = 0; exc_last.count = 0;
    exc_total = 0;
    expect_valid = 0; expect_cause = 0; expect_hit = 0;

    write_csr(mtvec, (xlen_t)(unsigned long)&_trap_entry_common);
}

void exception_set_action(xlen_t cause, exc_action_t action)
{
    if (cause >= CAUSE_MAX) return;
    exc_action[cause] = (unsigned char)action;
}

void exception_set_handler(xlen_t cause, exc_handler_t handler)
{
    if (cause >= CAUSE_MAX) return;
    exc_handler[cause] = handler;
    exc_action[cause]  = (unsigned char)EXC_ACT_HANDLER;
}

void exception_expect(xlen_t cause)
{
    expect_cause = cause;
    expect_valid = 1;
    expect_hit   = 0;
}

int exception_matched(void)
{
    int hit = expect_hit;
    expect_valid = 0;
    expect_hit   = 0;
    return hit;
}

const exception_record_t *exception_last(void) { return &exc_last; }
unsigned exception_count(void)                 { return exc_total; }

const char *exception_name(xlen_t cause)
{
    switch (cause) {
    case CAUSE_MISALIGNED_FETCH:    return "misaligned fetch";
    case CAUSE_FETCH_ACCESS:        return "fetch access fault";
    case CAUSE_ILLEGAL_INSTRUCTION: return "illegal instruction";
    case CAUSE_BREAKPOINT:          return "breakpoint";
    case CAUSE_MISALIGNED_LOAD:     return "misaligned load";
    case CAUSE_LOAD_ACCESS:         return "load access fault";
    case CAUSE_MISALIGNED_STORE:    return "misaligned store";
    case CAUSE_STORE_ACCESS:        return "store access fault";
    case CAUSE_USER_ECALL:          return "ecall from U";
    case CAUSE_SUPERVISOR_ECALL:    return "ecall from S";
    case CAUSE_MACHINE_ECALL:       return "ecall from M";
    default:                        return "unknown";
    }
}

/* -- Exception handling core -----------------------------------------
 * The order matters.
 *   1) Always record first. Whatever policy runs afterwards, the observation
 *      survives.
 *   2) If an expectation is registered and matches, SKIP takes priority,
 *      because the test's intent ("I raised this exception deliberately")
 *      outranks the policy.
 *   3) Otherwise follow the per-cause policy.
 */
static void handle_exception(xlen_t cause, trap_frame_t *frame)
{
    exc_action_t act;

    exc_total++;
    exc_last.cause = cause;
    exc_last.epc   = frame->mepc;
    exc_last.tval  = read_csr(mtval);
    exc_last.count = exc_total;

    if (expect_valid && cause == expect_cause) {
        expect_hit   = 1;
        expect_valid = 0;
        trap_skip_insn(frame);
        return;
    }

    act = (cause < CAUSE_MAX) ? (exc_action_t)exc_action[cause] : EXC_ACT_FATAL;

    switch (act) {
    case EXC_ACT_SKIP:
        trap_skip_insn(frame);
        return;

    case EXC_ACT_HANDLER:
        if (exc_handler[cause] && exc_handler[cause](cause, frame) == 0)
            return;
        break;

    case EXC_ACT_FATAL:
    default:
        break;
    }

    /* Reaching here means the exception is unhandled.
     * Returning with mepc unchanged would re-execute the same instruction and
     * trap forever, so we must die. exit() is implemented by rtdev_shim.c via
     * tohost. */
    exit(CODE_FATAL);
}

/* -- Trap entry point (called by trap_entry.S) -----------------------
 * The interrupt dispatch keeps the same shape as trap_handler() in syscalls.c,
 * so that CLINT/PLIC interrupts behave exactly as before even in tests that
 * enable the exception layer (pISR_BASE is used unchanged).
 */
void trap_dispatch(xlen_t mcause, trap_frame_t *frame)
{
    xlen_t ret;

    if (!TRAP_IS_INTERRUPT(mcause)) {
        handle_exception(TRAP_CAUSE_CODE(mcause), frame);
        return;
    }

    ret = 0;
    switch (TRAP_CAUSE_CODE(mcause)) {
    case IRQ_M_EXT: {
        xlen_t plic_claim = inpdw(PLIC_CLAIM);
        outpdw(PLIC_CLEAR, plic_claim);
        ret = pISR_BASE.ext_int_handler[plic_claim]();
        break;
    }
    case IRQ_M_TIMER:
        if (pISR_BASE.time_int_handler) ret = pISR_BASE.time_int_handler();
        break;
    case IRQ_M_SOFT:
        if (pISR_BASE.sw_int_handler) ret = pISR_BASE.sw_int_handler();
        break;
    default:
        /* U/S-mode interrupts or reserved codes. Never occur in an M-mode-only
         * configuration. */
        break;
    }

    if (ret != 0)
        exit(CODE_FAIL);
}
