/* trap_entry.S - shared trap entry point (for the exception handling layer)
 *
 * -- Why a new file instead of modifying mtvec.S ----------------------
 * 1) _trap_vector_entry in mtvec.S passes only one argument, mcause.
 *    For a C handler to change the resume address (mepc) it must reach the trap
 *    frame, and with no frame pointer passed in there is no way to do so. That
 *    is why trap_handler_PMP() in syscalls.c resorts to inline asm such as
 *    `asm("lw ra,12(sp)")`, which assumes "the size of the C function's own
 *    stack frame". That code breaks silently when the optimization level
 *    changes. New code does not use that approach.
 * 2) Modifying mtvec.S changes the code size inside .text.init and shifts every
 *    symbol address after it -> the images of all existing features would
 *    change, forcing the regression to be re-verified from scratch. No real
 *    benefit.
 * -> Hence "new file + opt-in". Only tests that list trap_entry in
 *    HAL_ASM_MODULES link this file. Existing cases are unaffected.
 *
 * -- Relationship to mtvec.S ------------------------------------------
 * The frame layout (-256, mepc@slot0, x1..x31@slot1..31) is exactly the same as
 * mtvec.S. The constants come from one place, Platform/Common/Inc/trap.h.
 * There is exactly one difference: the frame is passed to
 * trap_dispatch(mcause, frame).
 *
 * -- Usage -------------------------------------------------------------
 *   exceptions_init();          // internally swaps mtvec to _trap_entry_common
 * or directly:
 *   csrw mtvec, _trap_entry_common
 * The C-side entry function is trap_dispatch()
 * (Platform/Common/Src/exceptions.c).
 *
 * -- Constraints -------------------------------------------------------
 * - The FPU is not saved (rv32imac, no F extension).
 * - Nested traps are not supported (mstatus.MIE is not re-enabled).
 *   A second trap inside a handler overwrites mepc/mcause and breaks the
 *   return.
 */
#include "trap.h"

.option push
.option norelax

.section .text
.align 4
.global _trap_entry_common
_trap_entry_common:
    addi sp, sp, -TRAP_FRAME_SIZE

    sw   x1,   1*TRAP_REGBYTES(sp)
    sw   x2,   2*TRAP_REGBYTES(sp)
    sw   x3,   3*TRAP_REGBYTES(sp)
    sw   x4,   4*TRAP_REGBYTES(sp)
    sw   x5,   5*TRAP_REGBYTES(sp)
    sw   x6,   6*TRAP_REGBYTES(sp)
    sw   x7,   7*TRAP_REGBYTES(sp)
    sw   x8,   8*TRAP_REGBYTES(sp)
    sw   x9,   9*TRAP_REGBYTES(sp)
    sw   x10, 10*TRAP_REGBYTES(sp)
    sw   x11, 11*TRAP_REGBYTES(sp)
    sw   x12, 12*TRAP_REGBYTES(sp)
    sw   x13, 13*TRAP_REGBYTES(sp)
    sw   x14, 14*TRAP_REGBYTES(sp)
    sw   x15, 15*TRAP_REGBYTES(sp)
    sw   x16, 16*TRAP_REGBYTES(sp)
    sw   x17, 17*TRAP_REGBYTES(sp)
    sw   x18, 18*TRAP_REGBYTES(sp)
    sw   x19, 19*TRAP_REGBYTES(sp)
    sw   x20, 20*TRAP_REGBYTES(sp)
    sw   x21, 21*TRAP_REGBYTES(sp)
    sw   x22, 22*TRAP_REGBYTES(sp)
    sw   x23, 23*TRAP_REGBYTES(sp)
    sw   x24, 24*TRAP_REGBYTES(sp)
    sw   x25, 25*TRAP_REGBYTES(sp)
    sw   x26, 26*TRAP_REGBYTES(sp)
    sw   x27, 27*TRAP_REGBYTES(sp)
    sw   x28, 28*TRAP_REGBYTES(sp)
    sw   x29, 29*TRAP_REGBYTES(sp)
    sw   x30, 30*TRAP_REGBYTES(sp)
    sw   x31, 31*TRAP_REGBYTES(sp)

    /* slot 0 = mepc. It is read back from here on return, so a C handler that
     * modifies this slot changes the mret target (modifying the CSR has no
     * effect). */
    csrr t0, mepc
    sw   t0, TRAP_SLOT_MEPC*TRAP_REGBYTES(sp)

    /* trap_dispatch(xlen_t mcause, trap_frame_t *frame) */
    csrr a0, mcause
    mv   a1, sp
    jal  trap_dispatch

    /* Return: use the frame's mepc slot as the mret target */
    lw   t0, TRAP_SLOT_MEPC*TRAP_REGBYTES(sp)
    csrw mepc, t0

    /* Set mstatus.MPP to M-mode (11). Same handling as mtvec.S.
     * (The pre-trap mode is always M so MPP is already 11, but this follows the
     *  mtvec.S contract explicitly.) */
    li   t0, MSTATUS_MPP
    csrs mstatus, t0

    lw   x1,   1*TRAP_REGBYTES(sp)
    lw   x3,   3*TRAP_REGBYTES(sp)
    lw   x4,   4*TRAP_REGBYTES(sp)
    lw   x5,   5*TRAP_REGBYTES(sp)
    lw   x6,   6*TRAP_REGBYTES(sp)
    lw   x7,   7*TRAP_REGBYTES(sp)
    lw   x8,   8*TRAP_REGBYTES(sp)
    lw   x9,   9*TRAP_REGBYTES(sp)
    lw   x10, 10*TRAP_REGBYTES(sp)
    lw   x11, 11*TRAP_REGBYTES(sp)
    lw   x12, 12*TRAP_REGBYTES(sp)
    lw   x13, 13*TRAP_REGBYTES(sp)
    lw   x14, 14*TRAP_REGBYTES(sp)
    lw   x15, 15*TRAP_REGBYTES(sp)
    lw   x16, 16*TRAP_REGBYTES(sp)
    lw   x17, 17*TRAP_REGBYTES(sp)
    lw   x18, 18*TRAP_REGBYTES(sp)
    lw   x19, 19*TRAP_REGBYTES(sp)
    lw   x20, 20*TRAP_REGBYTES(sp)
    lw   x21, 21*TRAP_REGBYTES(sp)
    lw   x22, 22*TRAP_REGBYTES(sp)
    lw   x23, 23*TRAP_REGBYTES(sp)
    lw   x24, 24*TRAP_REGBYTES(sp)
    lw   x25, 25*TRAP_REGBYTES(sp)
    lw   x26, 26*TRAP_REGBYTES(sp)
    lw   x27, 27*TRAP_REGBYTES(sp)
    lw   x28, 28*TRAP_REGBYTES(sp)
    lw   x29, 29*TRAP_REGBYTES(sp)
    lw   x30, 30*TRAP_REGBYTES(sp)
    lw   x31, 31*TRAP_REGBYTES(sp)
    /* x2(sp) is not restored. The saved value is "sp after the -256", so
     * restoring it yields the same value, and the addi below returns it to its
     * original place.
     * (mtvec.S does restore x2, but the result is identical.) */
    addi sp, sp, TRAP_FRAME_SIZE
    mret

.option pop
