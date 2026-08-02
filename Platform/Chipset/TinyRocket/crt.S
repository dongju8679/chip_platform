/* crt.S - rt-dev crt.S adapted for chipyard (v301, Phase 1 DRAM)
 *
 * Changes from rt-dev original:
 *   - li sp, STACK_POINTER  ->  la sp, __stack_top   (chipyard linker symbol)
 *   - added gp setup + .bss clear (from v294 start.S; needed for chipyard)
 *   - _trap_vector_entry (mtvec.S) provides the rt-dev trap dispatch -> pISR_BASE
 *   - FPU off (RV32imac has no F ext); init_fpu skipped unless -DFPU
 */
.option push
.option norelax

.section .text.init
.global _start
_start:
    j _init_run

_init_run:
    jal _init_gpr
    jal _init_csr

    /* global pointer (chipyard) */
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    /* stack: chipyard linker symbol (was rt-dev STACK_POINTER) */
    la sp, __stack_top

    /* clear .bss (chipyard; rt-dev relied on ROM/preload) */
    la t0, __bss_start
    la t1, __bss_end
1:  bgeu t0, t1, 2f
    sw zero, 0(t0)
    addi t0, t0, 4
    j 1b
2:

    /* install rt-dev trap vector (mtvec.S) -> trap_handler -> pISR_BASE */
    la t0, _trap_vector_entry
    csrw mtvec, t0

    jal bp_init

    li a0, 1
    jal set_log
#ifdef FPU
    jal init_fpu
#endif
    jal main

    jal exit

_init_gpr:
    li x2, 0
    li x3, 0
    li x4, 0
    li x5, 0
    li x6, 0
    li x7, 0
    li x8, 0
    li x9, 0
    li x10, 0
    li x11, 0
    li x12, 0
    li x13, 0
    li x14, 0
    li x15, 0
    li x16, 0
    li x17, 0
    li x18, 0
    li x19, 0
    li x20, 0
    li x21, 0
    li x22, 0
    li x23, 0
    li x24, 0
    li x25, 0
    li x26, 0
    li x27, 0
    li x28, 0
    li x29, 0
    li x30, 0
    li x31, 0
    ret

_init_csr:
    csrw mtval, 0
    csrw mtvec, 0
    ret

.option pop
