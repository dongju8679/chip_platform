#include "../../../Common/Inc/encoding.h"

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
#define IGNORE_PSPEEDY_VALUE 0xf

.section .text.init
_preboot:
    // PLIC all disable
    li a5, PLIC_ENABLE
    sw zero, 0(a5)
    sw zero, 4(a5)
    // PLIC Hart Interrupt threshold set
    li a5, PLIC_THRESHOLD
    sw zero, 0(a5)
    // interrupt disable
    csrw mie, zero
    li a0, MSTATUS_MIE
    csrrc x0, mstatus, a0
    /* General purpose register initialization */
    .option push
    .option norelax
    la gp, __global_pointers
    .option pop
    la a0, exit
    csrw mtvec, a0
    j _init_run

_init_run:
    jal _timeout_init
    jal _delay_init
    li sp, STACK_POINTER
    csrw mtval, zero
    jal init
    jal user_main
    jal exit

    .global _delay_init
_delay_init:
    la a2, SYSTEM_STATUS_ADDR
    la a0, RFD_CPU_CTRL_ADDR
    lw a0, 0(a0)
    andi a0, a0, 1
    beqz a0, _52M
    la a1, NS_TO_CYCLE_76P8MHZ
    la a3, SYSTEM_STATUS_DELAY_76MHZ
    sw a3, 0(a2)
    j _delay_setting
_52M:
    la a1, NS_TO_CYCLE_52MHZ
    sw zero, 0(a2)
_delay_setting:
    la a0, ns_to_cycle
    sw a1, 0(a0)
    ret

    .global _timeout_init
_timeout_init:
    la a1, 0x80000400
    la a1, TIMEOUT_CONTROL_0
    sw a0, 0(a1)
    sw a0, 4(a1)
    sw a0, 8(a1)
    la a0, 0xDEAD0000
    sw a0, 12(a1)
    sw a0, 16(a1)
    sw a0, 20(a1)
    ret
