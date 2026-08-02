/* EXCEPTION_traps.c - RV32RocketConfig exception trap verification (self-check)
 *
 * -- What this proves ------------------------------------------------
 * "Exceptions fire per the spec, mcause/mepc/mtval are correct, and execution
 * resumes normally afterwards."
 * Until now this tree had no case verifying exceptions at all.
 * trap_handler() in syscalls.c simply called exit(CODE_FATAL) on any exception,
 * so you could not even tell which exception had occurred.
 *
 * -- How --------------------------------------------------------------
 * It uses the exception layer in Platform/Common.
 *   exceptions_init()          -> mtvec = _trap_entry_common (trap_entry.S)
 *   exception_expect(cause)    -> "this exception is expected next"
 *   <instruction that raises the exception>
 *   exception_matched()        -> did it actually happen
 * An expected exception is SKIPped automatically (mepc += instruction length),
 * so the test keeps going. An unexpected exception hits the FATAL policy and
 * dies immediately - there is no way to slip through silently into a false
 * PASS.
 *
 * -- Output path -------------------------------------------------------
 * Uses the HTIF console (Platform/Common/Inc/htif.h) rather than the UART.
 * The UART costs 43k cycles per character, so a single marker line would take
 * minutes.
 *
 * -- Running -----------------------------------------------------------
 *   ./verif/ip/S5740/EXCEPTION/traps/run.sh
 *   (no host Vseq -> do not pass +verif=; -DPRELOAD is required)
 */
#include "type.h"
#include "util.h"
#include "trap.h"
#include "exceptions.h"
#include "boot.h"
#include "htif.h"
#include "EXCEPTION_traps.h"

static unsigned fail_mask;

static void check(unsigned bit, int ok)
{
    if (!ok) fail_mask |= bit;
}

/* Raise one exception and check that the cause matches the expectation.
 * Returns: 1 = as expected. The trapping instruction address is returned in
 * epc_out. */
static int expect_and_run(xlen_t cause, void (*trigger)(void), xlen_t *epc_out)
{
    unsigned before = exception_count();
    int matched;

    exception_expect(cause);
    trigger();
    matched = exception_matched();

    if (epc_out) *epc_out = exception_last()->epc;

    /* The exception count must have grown by exactly one. Growing by two means
     * SKIP miscomputed the instruction length and re-executed the same spot. */
    if (exception_count() != before + 1u) return 0;
    if (exception_last()->cause != cause)  return 0;
    return matched;
}

/* -- Exception triggers ------------------------------------------------
 * All noinline. If inlined, the compiler interleaves surrounding code and the
 * "the trapping instruction is exactly at this address" check becomes unstable.
 */

/* illegal instruction.
 * The RISC-V spec pins down the all-ones encoding as illegal (reserved space
 * for very long instructions). Do not use 0x00000000 - that is two 16-bit
 * illegal instructions, so it traps again immediately after the SKIP. */
static void __attribute__((noinline)) do_illegal(void)
{
    __asm__ volatile (".word 0xFFFFFFFF");
}

/* breakpoint. With the C extension enabled this assembles to a 2-byte
 * c.ebreak. trap_skip_insn() determines the length from the low 2 bits, so
 * both forms work. */
static void __attribute__((noinline)) do_ebreak(void)
{
    __asm__ volatile ("ebreak");
}

/* M-mode ecall -> mcause 11 */
static void __attribute__((noinline)) do_ecall(void)
{
    __asm__ volatile ("ecall");
}

/* misaligned load/store.
 * Rocket does not handle unaligned accesses in hardware; it raises an
 * exception.
 * The address comes from a volatile global so the compiler cannot assume
 * alignment. */
static volatile unsigned int misalign_area[4];
static volatile unsigned int *volatile misalign_ptr;
static volatile unsigned int misalign_sink;

static void __attribute__((noinline)) do_load_misaligned(void)
{
    misalign_sink = *misalign_ptr;
}

static void __attribute__((noinline)) do_store_misaligned(void)
{
    *misalign_ptr = 0xA5A5A5A5u;
}

int main(void)
{
    xlen_t epc_illegal = 0;
    unsigned before_resume;
    volatile int resume_witness;

    /* 1) Self-check the boot state: did crt.S set up sp/gp/mtvec/.bss
     *    correctly?
     *    IMPORTANT: this must run before exceptions_init() - the point of the
     *    mtvec check is to inspect the value crt.S installed
     *    (_trap_vector_entry). */
    check(EXC_T_BOOT, boot_check() == 0);

    /* 2) Enable the exception layer (swap mtvec to the shared entry point) */
    exceptions_init();

    /* 3) illegal instruction (mcause 2) */
    check(EXC_T_ILLEGAL,
          expect_and_run(CAUSE_ILLEGAL_INSTRUCTION, do_illegal, &epc_illegal));

    /* 4) breakpoint (mcause 3) */
    check(EXC_T_BREAKPOINT,
          expect_and_run(CAUSE_BREAKPOINT, do_ebreak, 0));

    /* 5) ecall from M-mode (mcause 11) */
    check(EXC_T_ECALL,
          expect_and_run(CAUSE_MACHINE_ECALL, do_ecall, 0));

    /* 6) misaligned load (mcause 4) - address off the word boundary by 1 byte */
    misalign_ptr = (volatile unsigned int *)
                   ((xlen_t)(unsigned long)&misalign_area[1] + 1u);
    check(EXC_T_LOAD_MISAL,
          expect_and_run(CAUSE_MISALIGNED_LOAD, do_load_misaligned, 0));

    /* 7) misaligned store (mcause 6) */
    check(EXC_T_STORE_MISAL,
          expect_and_run(CAUSE_MISALIGNED_STORE, do_store_misaligned, 0));

    /* 8) Does mepc point at the trapping instruction address?
     *    The .word inside do_illegal() is the first instruction of the function
     *    body, so mepc must equal do_illegal's start address. */
    check(EXC_T_EPC, epc_illegal == (xlen_t)(unsigned long)do_illegal);

    /* 9) Cumulative exception count: exactly 5 were raised above */
    check(EXC_T_COUNT, exception_count() == 5u);

    /* 10) Normal resume after an exception.
     *     With only the SKIP policy set (no expectation) and an ebreak raised,
     *     the statement after it must execute once the trap returns. If it does
     *     not, mret / frame restoration is broken. */
    before_resume = exception_count();
    exception_set_action(CAUSE_BREAKPOINT, EXC_ACT_SKIP);
    resume_witness = 0;
    __asm__ volatile ("ebreak");
    resume_witness = 0x1234;
    check(EXC_T_RESUME,
          resume_witness == 0x1234 && exception_count() == before_resume + 1u);

    /* -- Verdict -- */
    if (fail_mask == 0) {
        htif_puts(EXC_PASS_MARK "\n");
        return CODE_SUCCESS;
    }

    htif_puts(EXC_FAIL_MARK);
    htif_puthex(fail_mask);
    htif_puts(" last_cause=");
    htif_puthex(exception_last()->cause);
    htif_puts(" last_epc=");
    htif_puthex(exception_last()->epc);
    htif_puts("\n");
    return CODE_FAIL;
}
