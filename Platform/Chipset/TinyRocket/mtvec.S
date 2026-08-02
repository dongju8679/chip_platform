#include "encoding.h"

#if __riscv_xlen == 64
# define FLREG fld
# define FSREG fsd
# define LREG ld
# define SREG sd
# define REGBYTES 8
#elif __riscv_xlen == 32
# define FLREG flw
# define FSREG fsw
# define LREG lw
# define SREG sw
# define REGBYTES 4
#else
# error "No __riscv_xlen defined. "
#endif

.option push
.option norelax

.section .text.init

.align 4
.global _trap_vector_entry
_trap_vector_entry:
    addi sp, sp, -256
    SREG x1, 1*REGBYTES(sp)
    SREG x2, 2*REGBYTES(sp)
    SREG x3, 3*REGBYTES(sp)
    SREG x4, 4*REGBYTES(sp)
    SREG x5, 5*REGBYTES(sp)
    SREG x6, 6*REGBYTES(sp)
    SREG x7, 7*REGBYTES(sp)
    SREG x8, 8*REGBYTES(sp)
    SREG x9, 9*REGBYTES(sp)
    SREG x10, 10*REGBYTES(sp)
    SREG x11, 11*REGBYTES(sp)
    SREG x12, 12*REGBYTES(sp)
    SREG x13, 13*REGBYTES(sp)
    SREG x14, 14*REGBYTES(sp)
    SREG x15, 15*REGBYTES(sp)
    SREG x16, 16*REGBYTES(sp)
    SREG x17, 17*REGBYTES(sp)
    SREG x18, 18*REGBYTES(sp)
    SREG x19, 19*REGBYTES(sp)
    SREG x20, 20*REGBYTES(sp)
    SREG x21, 21*REGBYTES(sp)
    SREG x22, 22*REGBYTES(sp)
    SREG x23, 23*REGBYTES(sp)
    SREG x24, 24*REGBYTES(sp)
    SREG x25, 25*REGBYTES(sp)
    SREG x26, 26*REGBYTES(sp)
    SREG x27, 27*REGBYTES(sp)
    SREG x28, 28*REGBYTES(sp)
    SREG x29, 29*REGBYTES(sp)
    SREG x30, 30*REGBYTES(sp)
    SREG x31, 31*REGBYTES(sp)
#ifdef FPU
    FSREG f0, 32*REGBYTES(sp)
    FSREG f1, 33*REGBYTES(sp)
    FSREG f2, 34*REGBYTES(sp)
    FSREG f3, 35*REGBYTES(sp)
    FSREG f4, 36*REGBYTES(sp)
    FSREG f5, 37*REGBYTES(sp)
    FSREG f6, 38*REGBYTES(sp)
    FSREG f7, 39*REGBYTES(sp)
    FSREG f8, 40*REGBYTES(sp)
    FSREG f9, 41*REGBYTES(sp)
    FSREG f10, 42*REGBYTES(sp)
    FSREG f11, 43*REGBYTES(sp)
    FSREG f12, 44*REGBYTES(sp)
    FSREG f13, 45*REGBYTES(sp)
    FSREG f14, 46*REGBYTES(sp)
    FSREG f15, 47*REGBYTES(sp)
    FSREG f16, 48*REGBYTES(sp)
    FSREG f17, 49*REGBYTES(sp)
    FSREG f18, 50*REGBYTES(sp)
    FSREG f19, 51*REGBYTES(sp)
    FSREG f20, 52*REGBYTES(sp)
    FSREG f21, 53*REGBYTES(sp)
    FSREG f22, 54*REGBYTES(sp)
    FSREG f23, 55*REGBYTES(sp)
    FSREG f24, 56*REGBYTES(sp)
    FSREG f25, 57*REGBYTES(sp)
    FSREG f26, 58*REGBYTES(sp)
    FSREG f27, 59*REGBYTES(sp)
    FSREG f28, 60*REGBYTES(sp)
    FSREG f29, 61*REGBYTES(sp)
    FSREG f30, 62*REGBYTES(sp)
    FSREG f31, 63*REGBYTES(sp)
#endif
    csrr a0, mepc
    SREG a0, 0*REGBYTES(sp)

    csrr a0, mcause

    jal trap_handler

_trap_vector_ret:
    li a0, MSTATUS_MPP
    csrrs x0, mstatus, a0

    LREG a0, 0*REGBYTES(sp)
    csrw mepc, a0

#ifdef PREEMPTION
    LREG x1, 1*REGBYTES(sp)
    LREG x2, 2*REGBYTES(sp)
    LREG x3, 3*REGBYTES(sp)
    LREG x4, 4*REGBYTES(sp)
    LREG x5, 5*REGBYTES(sp)
    LREG x6, 6*REGBYTES(sp)
    LREG x7, 7*REGBYTES(sp)
    LREG x8, 8*REGBYTES(sp)
    LREG x9, 9*REGBYTES(sp)
    LREG x10, 10*REGBYTES(sp)
    LREG x11, 11*REGBYTES(sp)
    LREG x12, 12*REGBYTES(sp)
    LREG x13, 13*REGBYTES(sp)
    LREG x14, 14*REGBYTES(sp)
    LREG x15, 15*REGBYTES(sp)
    LREG x16, 16*REGBYTES(sp)
    LREG x17, 17*REGBYTES(sp)
    LREG x18, 18*REGBYTES(sp)
    LREG x19, 19*REGBYTES(sp)
    LREG x20, 20*REGBYTES(sp)
    LREG x21, 21*REGBYTES(sp)
    LREG x22, 22*REGBYTES(sp)
    LREG x23, 23*REGBYTES(sp)
    LREG x24, 24*REGBYTES(sp)
    LREG x25, 25*REGBYTES(sp)
    LREG x26, 26*REGBYTES(sp)
    LREG x27, 27*REGBYTES(sp)
    LREG x28, 28*REGBYTES(sp)
    LREG x29, 29*REGBYTES(sp)
    LREG x30, 30*REGBYTES(sp)
    LREG x31, 31*REGBYTES(sp)
#ifdef FPU
    FLREG f0, 32*REGBYTES(sp)
    FLREG f1, 33*REGBYTES(sp)
    FLREG f2, 34*REGBYTES(sp)
    FLREG f3, 35*REGBYTES(sp)
    FLREG f4, 36*REGBYTES(sp)
    FLREG f5, 37*REGBYTES(sp)
    FLREG f6, 38*REGBYTES(sp)
    FLREG f7, 39*REGBYTES(sp)
    FLREG f8, 40*REGBYTES(sp)
    FLREG f9, 41*REGBYTES(sp)
    FLREG f10, 42*REGBYTES(sp)
    FLREG f11, 43*REGBYTES(sp)
    FLREG f12, 44*REGBYTES(sp)
    FLREG f13, 45*REGBYTES(sp)
    FLREG f14, 46*REGBYTES(sp)
    FLREG f15, 47*REGBYTES(sp)
    FLREG f16, 48*REGBYTES(sp)
    FLREG f17, 49*REGBYTES(sp)
    FLREG f18, 50*REGBYTES(sp)
    FLREG f19, 51*REGBYTES(sp)
    FLREG f20, 52*REGBYTES(sp)
    FLREG f21, 53*REGBYTES(sp)
    FLREG f22, 54*REGBYTES(sp)
    FLREG f23, 55*REGBYTES(sp)
    FLREG f24, 56*REGBYTES(sp)
    FLREG f25, 57*REGBYTES(sp)
    FLREG f26, 58*REGBYTES(sp)
    FLREG f27, 59*REGBYTES(sp)
    FLREG f28, 60*REGBYTES(sp)
    FLREG f29, 61*REGBYTES(sp)
    FLREG f30, 62*REGBYTES(sp)
    FLREG f31, 63*REGBYTES(sp)
#endif
    addi sp, sp, 256
    mret
#endif

.align 4
.global _trap_vector_entry_PMP
_trap_vector_entry_PMP:
    addi sp, sp, -256
    SREG x1, 1*REGBYTES(sp)
    SREG x2, 2*REGBYTES(sp)
    SREG x3, 3*REGBYTES(sp)
    SREG x4, 4*REGBYTES(sp)
    SREG x5, 5*REGBYTES(sp)
    SREG x6, 6*REGBYTES(sp)
    SREG x7, 7*REGBYTES(sp)
    SREG x8, 8*REGBYTES(sp)
    SREG x9, 9*REGBYTES(sp)
    SREG x10, 10*REGBYTES(sp)
    SREG x11, 11*REGBYTES(sp)
    SREG x12, 12*REGBYTES(sp)
    SREG x13, 13*REGBYTES(sp)
    SREG x14, 14*REGBYTES(sp)
    SREG x15, 15*REGBYTES(sp)
    SREG x16, 16*REGBYTES(sp)
    SREG x17, 17*REGBYTES(sp)
    SREG x18, 18*REGBYTES(sp)
    SREG x19, 19*REGBYTES(sp)
    SREG x20, 20*REGBYTES(sp)
    SREG x21, 21*REGBYTES(sp)
    SREG x22, 22*REGBYTES(sp)
    SREG x23, 23*REGBYTES(sp)
    SREG x24, 24*REGBYTES(sp)
    SREG x25, 25*REGBYTES(sp)
    SREG x26, 26*REGBYTES(sp)
    SREG x27, 27*REGBYTES(sp)
    SREG x28, 28*REGBYTES(sp)
    SREG x29, 29*REGBYTES(sp)
    SREG x30, 30*REGBYTES(sp)
    SREG x31, 31*REGBYTES(sp)
#ifdef FPU
    FSREG f0, 32*REGBYTES(sp)
    FSREG f1, 33*REGBYTES(sp)
    FSREG f2, 34*REGBYTES(sp)
    FSREG f3, 35*REGBYTES(sp)
    FSREG f4, 36*REGBYTES(sp)
    FSREG f5, 37*REGBYTES(sp)
    FSREG f6, 38*REGBYTES(sp)
    FSREG f7, 39*REGBYTES(sp)
    FSREG f8, 40*REGBYTES(sp)
    FSREG f9, 41*REGBYTES(sp)
    FSREG f10, 42*REGBYTES(sp)
    FSREG f11, 43*REGBYTES(sp)
    FSREG f12, 44*REGBYTES(sp)
    FSREG f13, 45*REGBYTES(sp)
    FSREG f14, 46*REGBYTES(sp)
    FSREG f15, 47*REGBYTES(sp)
    FSREG f16, 48*REGBYTES(sp)
    FSREG f17, 49*REGBYTES(sp)
    FSREG f18, 50*REGBYTES(sp)
    FSREG f19, 51*REGBYTES(sp)
    FSREG f20, 52*REGBYTES(sp)
    FSREG f21, 53*REGBYTES(sp)
    FSREG f22, 54*REGBYTES(sp)
    FSREG f23, 55*REGBYTES(sp)
    FSREG f24, 56*REGBYTES(sp)
    FSREG f25, 57*REGBYTES(sp)
    FSREG f26, 58*REGBYTES(sp)
    FSREG f27, 59*REGBYTES(sp)
    FSREG f28, 60*REGBYTES(sp)
    FSREG f29, 61*REGBYTES(sp)
    FSREG f30, 62*REGBYTES(sp)
    FSREG f31, 63*REGBYTES(sp)
#endif
    csrr a0, mepc
    SREG a0, 0*REGBYTES(sp)
    csrr a0, mcause

    jal trap_handler_PMP
    jal _trap_vector_ret
    jal _trap_vector_ret_PMP
    addi sp, sp, 272
    jal _trap_vector_ret

_trap_vector_ret_PMP:
    li a0, MSTATUS_MPP
    csrrs x0, mstatus, a0
    LREG a0, 0*REGBYTES(sp)
    addi a0, a0, 2
    csrw mepc, a0

#ifdef PREEMPTION
    LREG x1, 1*REGBYTES(sp)
    LREG x2, 2*REGBYTES(sp)
    LREG x3, 3*REGBYTES(sp)
    LREG x4, 4*REGBYTES(sp)
    LREG x5, 5*REGBYTES(sp)
    LREG x6, 6*REGBYTES(sp)
    LREG x7, 7*REGBYTES(sp)
    LREG x8, 8*REGBYTES(sp)
    LREG x9, 9*REGBYTES(sp)
    LREG x10, 10*REGBYTES(sp)
    LREG x11, 11*REGBYTES(sp)
    LREG x12, 12*REGBYTES(sp)
    LREG x13, 13*REGBYTES(sp)
    LREG x14, 14*REGBYTES(sp)
    LREG x15, 15*REGBYTES(sp)
    LREG x16, 16*REGBYTES(sp)
    LREG x17, 17*REGBYTES(sp)
    LREG x18, 18*REGBYTES(sp)
    LREG x19, 19*REGBYTES(sp)
    LREG x20, 20*REGBYTES(sp)
    LREG x21, 21*REGBYTES(sp)
    LREG x22, 22*REGBYTES(sp)
    LREG x23, 23*REGBYTES(sp)
    LREG x24, 24*REGBYTES(sp)
    LREG x25, 25*REGBYTES(sp)
    LREG x26, 26*REGBYTES(sp)
    LREG x27, 27*REGBYTES(sp)
    LREG x28, 28*REGBYTES(sp)
    LREG x29, 29*REGBYTES(sp)
    LREG x30, 30*REGBYTES(sp)
    LREG x31, 31*REGBYTES(sp)
#ifdef FPU
    FLREG f0, 32*REGBYTES(sp)
    FLREG f1, 33*REGBYTES(sp)
    FLREG f2, 34*REGBYTES(sp)
    FLREG f3, 35*REGBYTES(sp)
    FLREG f4, 36*REGBYTES(sp)
    FLREG f5, 37*REGBYTES(sp)
    FLREG f6, 38*REGBYTES(sp)
    FLREG f7, 39*REGBYTES(sp)
    FLREG f8, 40*REGBYTES(sp)
    FLREG f9, 41*REGBYTES(sp)
    FLREG f10, 42*REGBYTES(sp)
    FLREG f11, 43*REGBYTES(sp)
    FLREG f12, 44*REGBYTES(sp)
    FLREG f13, 45*REGBYTES(sp)
    FLREG f14, 46*REGBYTES(sp)
    FLREG f15, 47*REGBYTES(sp)
    FLREG f16, 48*REGBYTES(sp)
    FLREG f17, 49*REGBYTES(sp)
    FLREG f18, 50*REGBYTES(sp)
    FLREG f19, 51*REGBYTES(sp)
    FLREG f20, 52*REGBYTES(sp)
    FLREG f21, 53*REGBYTES(sp)
    FLREG f22, 54*REGBYTES(sp)
    FLREG f23, 55*REGBYTES(sp)
    FLREG f24, 56*REGBYTES(sp)
    FLREG f25, 57*REGBYTES(sp)
    FLREG f26, 58*REGBYTES(sp)
    FLREG f27, 59*REGBYTES(sp)
    FLREG f28, 60*REGBYTES(sp)
    FLREG f29, 61*REGBYTES(sp)
    FLREG f30, 62*REGBYTES(sp)
    FLREG f31, 63*REGBYTES(sp)
#endif
    addi sp, sp, 256
    mret
#endif
    
