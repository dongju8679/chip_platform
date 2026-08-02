#ifndef HAL_H
#define HAL_H

#include <stdint.h>

/* -- UART ------------------------------- */
#define UART_BASE    0x54000000
#define UART_TXDATA  (*(volatile uint32_t*)(UART_BASE + 0x00))
#define UART_RXDATA  (*(volatile uint32_t*)(UART_BASE + 0x04))
#define UART_TXCTRL  (*(volatile uint32_t*)(UART_BASE + 0x08))
#define UART_RXCTRL  (*(volatile uint32_t*)(UART_BASE + 0x0C))

static inline void uart_putc(char c) {
    while (UART_TXDATA & 0x80000000);
    UART_TXDATA = c;
}

static inline void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

/* -- CLINT ------------------------------ */
#define CLINT_BASE    0x02000000
#define CLINT_MSIP    (*(volatile uint32_t*)(CLINT_BASE + 0x0000))
#define CLINT_MTIMECMP (*(volatile uint64_t*)(CLINT_BASE + 0x4000))
#define CLINT_MTIME   (*(volatile uint64_t*)(CLINT_BASE + 0xBFF8))

static inline uint64_t get_time(void) {
    return CLINT_MTIME;
}

/* -- PASS/FAIL -------------------------- */
#define TOHOST_ADDR 0x80001000
#define TEST_PASS() do { \
    volatile uint32_t *t = (uint32_t*)TOHOST_ADDR; \
    *t = 1; \
    while(1); \
} while(0)

#define TEST_FAIL(n) do { \
    volatile uint32_t *t = (uint32_t*)TOHOST_ADDR; \
    *t = ((n) << 1) | 1; \
    while(1); \
} while(0)

#endif /* HAL_H */
