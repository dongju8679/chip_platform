/* Common startup for all custom tests */
#include "encoding.h"

.section .text.init, "ax"
.globl _start
_start:
  /* set trap vector */
  la    t0, _trap_handler
  csrw  mtvec, t0


  /* clear registers */
  li    x1, 0
  li    x2, 0
  li    x3, 0
  li    x4, 0
  li    x5, 0
  li    x6, 0
  li    x7, 0
  li    x8, 0
  li    x9, 0
  li    x10, 0
  li    x11, 0
  li    x12, 0
  li    x13, 0
  li    x14, 0
  li    x15, 0

  /* set stack pointer */
  la    sp, _stack_top

  /* call test main */
  jal   ra, test_main

  /* if test_main returns 0, pass */
  beqz  a0, _pass
  /* else fail with returned test number */
  slli  a0, a0, 1
  ori   a0, a0, 1
  j     _write_tohost

_pass:
  li    a0, 1

_write_tohost:
  la    t0, tohost
  sw    a0, 0(t0)
1:
  j     1b

/* default trap handler - unexpected trap = fail */
.align 2
_trap_handler:
  csrr  t0, mcause
  li    t1, 0x80000000    /* check if interrupt */
  and   t2, t0, t1
  bnez  t2, _interrupt_handler
  /* exception: fail with cause in gp */
  addi  gp, t0, 256       /* offset to distinguish from test failures */
  slli  gp, gp, 1
  ori   gp, gp, 1
  la    t0, tohost
  sw    gp, 0(t0)
1:
  j     1b

/* interrupt handler - can be overridden by tests */
.weak _interrupt_handler
_interrupt_handler:
  mret

.section .tohost, "aw"
.globl tohost
.globl fromhost
.align 4
tohost:   .word 0
fromhost: .word 0
