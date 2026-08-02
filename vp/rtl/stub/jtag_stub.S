# jtag_stub.S - a "spin quietly" firmware for JTAG debugging
#
# Why it is needed: on the chipyard harness, TSI loads and runs the ELF. Given
# real firmware, the program finishes before OpenOCD can even complete target
# examine; tohost gets written and the simulation hits $stop -> broken pipe on the JTAG socket.
# So a stub that never terminates is loaded instead, and the real firmware is
# loaded through the Debug Module via GDB's `load` (the same flow as flashing with a debugger on real silicon).
    .section .text.init,"ax",@progbits
    .globl _start
_start:
    li   t0, 0
spin:
    addi t0, t0, 1
    j    spin

    .section .tohost,"aw",@nobits
    .align 6
    .globl tohost
tohost:  .dword 0
    .globl fromhost
fromhost: .dword 0
