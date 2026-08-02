#ifndef __TRAP__
#define __TRAP__

/* trap.h - shared definitions for trap entry/return (RV32, M-mode)
 *
 * -- Why this file exists --------------------------------------------
 * Trap-related knowledge used to be scattered across three places:
 *   1) Platform/Chipset/TinyRocket/mtvec.S : trap frame layout (-256, mepc@0)
 *   2) Platform/Common/Src/syscalls.c      : mcause dispatch + inline asm
 *                                            mepc fixup
 *   3) Platform/Common/Inc/interrupt.h     : interrupt cause (_INT_*) constants
 * As a result, knowledge that depends on the assembly frame layout - such as
 * "to advance mepc by 4 you must poke sp+12" - was hardcoded into C code.
 * This header records that contract in one place.
 *
 * -- Consistency with crt.S / mtvec.S (must be changed together) ------
 * crt.S:  la t0, _trap_vector_entry ; csrw mtvec, t0
 *         -> mtvec MODE=0 (Direct). Every trap enters at one address.
 *            Not vectored mode, so C dispatches on mcause.
 * mtvec.S(_trap_vector_entry):
 *         addi sp, sp, -TRAP_FRAME_SIZE
 *         SREG x1..x31 -> 1..31 * REGBYTES(sp)
 *         csrr a0, mepc ; SREG a0, 0(sp)      <- mepc occupies frame slot 0
 *         csrr a0, mcause ; jal trap_handler  <- mcause is the only argument
 *         (return) LREG a0, 0(sp) ; csrw mepc, a0 ; restore ; mret
 *
 * IMPORTANT: just before returning, frame slot 0 is written back into mepc.
 *   So calling write_csr(mepc, ...) inside a handler is pointless - it gets
 *   overwritten. To change the resume address you must modify "frame slot 0,
 *   not the CSR".
 *   trap_handler_PMP() in syscalls.c does exactly that via
 *       asm("lw ra,12(sp)"); asm("add ra,ra,2"); asm("sw ra,12(sp)")
 *   but the 12(sp) offset depends on the stack frame size the compiler chose -
 *   change the optimization level and it silently pokes the wrong place.
 *   New code therefore does not use that approach. Instead trap_entry.S passes
 *   the frame pointer as an argument and C accesses it via trap_frame_t.
 *   (See Platform/Common/Src/trap_entry.S and exceptions.c)
 *
 * -- Is this verifiable on chipyard? ---------------------------------
 * This file holds only constants and inline definitions, so it has no
 * executable code of its own. Whether the contract written here is correct is
 * nonetheless confirmed by execution:
 *   - frame layout     -> correct if execution resumes normally after an
 *                         exception (EXC_T_RESUME)
 *   - trap_insn_len()  -> correct if, after trapping on a mix of compressed and
 *                         uncompressed instructions, the exception count grows
 *                         by exactly 1 per SKIP (EXC_T_COUNT)
 * verif/ip/S5740/EXCEPTION/traps checks both.
 */

/* encoding.h is written so it can also be included from assembly (mtvec.S
 * already does). type.h holds typedefs and breaks when included from a .S, so
 * it is pulled in only on the C side.
 * -> This header is therefore includable from .S too (the point is sharing the
 *    frame constants). */
#include "encoding.h"
#ifndef __ASSEMBLER__
#include "type.h"
#endif

/* -- Trap frame -----------------------------------------------------
 * Layout shared by mtvec.S and trap_entry.S. Never change it
 * (changing it requires fixing both .S files and this header together).
 *   slot 0        : mepc (resume address)
 *   slot 1..31    : x1..x31
 *   slot 32..63   : f0..f31 (FPU builds only; unused on rv32imac)
 * The frame size is fixed at 256 bytes regardless of FPU presence.
 */
#define TRAP_REGBYTES     4
#define TRAP_FRAME_SIZE   256
#define TRAP_SLOT_MEPC    0
#define TRAP_SLOT_X(n)    (n)          /* x1 -> 1, ... x31 -> 31 */

#ifndef __ASSEMBLER__

/* The trap frame as seen from C. trap_entry.S passes the address of this
 * struct as the second argument. Slot 0 is mepc, so it gets its own name. */
typedef struct _trap_frame {
    xlen_t mepc;        /* slot 0  : resume address. Writing here changes the mret target */
    xlen_t x[31];       /* slot 1..31 : x1(ra) .. x31(t6). x[0] is x1 */
} trap_frame_t;

/* Accessors for commonly used registers (x[0]=x1, so indices are off by one) */
#define TF_RA(f)   ((f)->x[0])          /* x1  */
#define TF_SP(f)   ((f)->x[1])          /* x2  */
#define TF_A0(f)   ((f)->x[9])          /* x10 */
#define TF_A1(f)   ((f)->x[10])         /* x11 */

/* -- mcause ---------------------------------------------------------
 * MSB set means interrupt, clear means exception. RV32, so bit31.
 * (Same value as _INT_BIT in interrupt.h. Duplicated here so files that do not
 *  include interrupt.h can still use it.)
 */
#define TRAP_INT_BIT        (1u << 31)
#define TRAP_IS_INTERRUPT(c) (((xlen_t)(c) & TRAP_INT_BIT) != 0)
#define TRAP_CAUSE_CODE(c)   ((xlen_t)(c) & ~TRAP_INT_BIT)

/* -- Exception cause codes (RISC-V Privileged Spec, Machine cause register) --
 *
 * PITFALL: there are two copies of encoding.h in the tree and their names
 *   differ.
 *     A) Platform/Chipset/TinyRocket/hal_shim/encoding.h  4355 lines (recent
 *        riscv-opcodes)
 *          -> CAUSE_FETCH_ACCESS / CAUSE_LOAD_ACCESS / CAUSE_STORE_ACCESS
 *     B) Platform/Common/Inc/encoding.h                   1335 lines (older)
 *          -> CAUSE_FAULT_FETCH  / CAUSE_FAULT_LOAD   / CAUSE_FAULT_STORE
 *   Same values (1/5/7), different names. The remaining CAUSE_x names agree.
 *
 *   Which one wins is decided first by "the directory of the file that wrote
 *   the #include", not by -I order (the quoted-include rule).
 *     - included by a header inside Platform/Common/Inc/ -> B (same folder)
 *     - included by any other file                       -> A (hal_shim comes
 *                                                            first in -I order)
 *   So this header (trap.h) sees B while test sources see A.
 *   This is what actually broke exceptions.c with an undefined
 *   CAUSE_LOAD_ACCESS.
 *
 *   Therefore only "values fixed by the spec" are trusted, and the names are
 *   pinned down here. The three names that diverge get individual guards so
 *   both spellings are defined.
 *   (Merging encoding.h into a single copy is a follow-up task.)
 */
#ifndef CAUSE_MISALIGNED_FETCH
#define CAUSE_MISALIGNED_FETCH      0x0
#define CAUSE_ILLEGAL_INSTRUCTION   0x2
#define CAUSE_BREAKPOINT            0x3
#define CAUSE_MISALIGNED_LOAD       0x4
#define CAUSE_MISALIGNED_STORE      0x6
#define CAUSE_USER_ECALL            0x8
#define CAUSE_SUPERVISOR_ECALL      0x9
#define CAUSE_MACHINE_ECALL         0xb
#endif

/* The three divergent names. Make both spellings usable whichever encoding.h
 * was picked up. */
#ifndef CAUSE_FETCH_ACCESS
#define CAUSE_FETCH_ACCESS          0x1
#endif
#ifndef CAUSE_LOAD_ACCESS
#define CAUSE_LOAD_ACCESS           0x5
#endif
#ifndef CAUSE_STORE_ACCESS
#define CAUSE_STORE_ACCESS          0x7
#endif
#ifndef CAUSE_FAULT_FETCH
#define CAUSE_FAULT_FETCH           0x1
#endif
#ifndef CAUSE_FAULT_LOAD
#define CAUSE_FAULT_LOAD            0x5
#endif
#ifndef CAUSE_FAULT_STORE
#define CAUSE_FAULT_STORE           0x7
#endif

/* Page faults are absent from the older encoding.h (Sv32 is unused, so they
 * never occur in this configuration). */
#ifndef CAUSE_FETCH_PAGE_FAULT
#define CAUSE_FETCH_PAGE_FAULT      0xc
#define CAUSE_LOAD_PAGE_FAULT       0xd
#define CAUSE_STORE_PAGE_FAULT      0xf
#endif

#define CAUSE_MAX                   16u

/* Interrupt cause codes (low bits of mcause) */
#ifndef IRQ_M_SOFT
#define IRQ_M_SOFT      3
#define IRQ_M_TIMER     7
#define IRQ_M_EXT      11
#endif

/* -- Length of the trapping instruction ------------------------------
 * To "ignore the exception and resume at the next instruction" mepc must be
 * advanced by the instruction length. RV32IMAC mixes in compressed (2-byte)
 * instructions, so blindly adding 4 would jump into the middle of an
 * instruction after a compressed one.
 * RISC-V instruction length rule: if the low 2 bits are not 11, it is a 16-bit
 * instruction. Only the low 2 bits of the word mepc points at matter.
 *
 * Caution: this re-reads the faulting address, so that address must be
 *          readable. (Do not use it for cases like a fetch access fault where
 *          it cannot be read.)
 */
static inline unsigned int trap_insn_len(xlen_t pc)
{
    unsigned short lo = *(volatile unsigned short *)pc;
    return ((lo & 0x3u) == 0x3u) ? 4u : 2u;
}

/* Advance the frame's resume address past the trapping instruction.
 * Used when the policy is "record it and move on", as for ecall/ebreak/illegal.
 * (Leaving mepc unchanged would re-execute the same instruction and trap
 *  forever.) */
static inline void trap_skip_insn(trap_frame_t *f)
{
    f->mepc += trap_insn_len(f->mepc);
}

#endif /* __ASSEMBLER__ */
#endif /* __TRAP__ */
