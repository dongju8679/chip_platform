#ifndef _CUSTOM_COMMON_H
#define _CUSTOM_COMMON_H

/* tohost/fromhost interface for test pass/fail */
#define TOHOST_ADDR 0x80001000

#define TEST_PASS     \
  li gp, 1;           \
  li t0, TOHOST_ADDR; \
  sw gp, 0(t0);       \
  j .

#define TEST_FAIL(n)       \
  li gp, ((n << 1) | 1);  \
  li t0, TOHOST_ADDR;     \
  sw gp, 0(t0);           \
  j .

#endif
