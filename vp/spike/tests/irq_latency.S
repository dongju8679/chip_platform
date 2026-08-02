# irq_latency.S - minimal firmware for measuring trap/interrupt latency (spike ED only)
#
# Proves the (trigger_irq -> get_cycle delta) path of verif_ca_run end to end:
#   1. Configure mtvec/mie.MSIE/mstatus.MIE, then write BP_TEST_begin to the mailbox
#   2. The host (verif_ca_run) sees begin, records cycle_start -> trigger_irq(3)
#      (spike backend: sets state.mip.MSIP directly)
#   3. The next step takes the interrupt trap -> tick() detects it via "minstret unchanged"
#      and accumulates timing_model.account_trap() (trap_pen)
#   4. When the ISR writes BP_TEST_end, the host records cycle_end
#      -> measured = cycle_end - cycle_start = the model cycles up to ISR entry + the BP write
#   5. The ISR terminates with tohost=1 (the riscv-tests convention)
#
# mailbox = 0x80010000 (the chipyard-track convention; paired with verif_ca_run_dram,
#   which is verif_ca_run built with -DVERIF_BREAK_ADDR=0x80010000)
# Build: vp/spike/tests/build_irq_test.sh

    .section .text
    .globl _start
_start:
    la    t0, trap_handler
    csrw  mtvec, t0
    li    t0, 8                  # MSIE (bit 3)
    csrs  mie, t0
    li    t1, 0x80010000         # BREAK_POINT mailbox
    li    t2, 0xFFFFFFF2         # BP_TEST_begin
    sw    t2, 0(t1)
    csrsi mstatus, 8             # mstatus.MIE
1:  j     1b                     # wait for the interrupt (the host sets mip.MSIP)

    .align 2
trap_handler:
    li    t1, 0x80010000
    li    t2, 0xFFFFFFF5         # BP_TEST_end
    sw    t2, 0(t1)
    la    t1, tohost             # terminate (tohost=1 -> PASS)
    li    t2, 1
    sw    t2, 0(t1)
    sw    zero, 4(t1)
2:  j     2b

    .section .data
    .align 3
    .globl tohost
tohost:     .dword 0
    .globl fromhost
fromhost:   .dword 0
