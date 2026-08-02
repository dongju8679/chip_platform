/* uart.c - chipyard SiFive UART0 driver (v301)
 *
 * The existing IP verification path (mailbox handshake) is left completely
 * untouched. This file is an additional path for the DUT itself to print
 * stdout over the UART.
 */
#include "uart.h"

void uart_init(void)
{
    /* div gets its reset value of pbus_freq/115200 at elaboration time, so it
     * is left alone (RV32RocketConfig: 500MHz/115200 = 4340).
     * Only txen needs enabling. nstop=0 -> 1 stop bit (the harness UARTAdapter
     * also uses nstop=0). */
    UART_REG(UART_REG_TXCTRL) = UART_TXCTRL_TXEN;
    UART_REG(UART_REG_RXCTRL) = UART_RXCTRL_RXEN;
}

void uart_putc(char c)
{
    /* Poll until txdata bit31 (full) clears, then write */
    while (UART_REG(UART_REG_TXDATA) & UART_TXDATA_FULL) {
        /* busy wait */
    }
    UART_REG(UART_REG_TXDATA) = (unsigned int)(unsigned char)c;
}

void uart_puts(const char *s)
{
    if (!s) return;
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

void uart_write(const char *buf, unsigned int len)
{
    unsigned int i;
    if (!buf) return;
    for (i = 0; i < len; i++) {
        if (buf[i] == '\n') uart_putc('\r');
        uart_putc(buf[i]);
    }
}

static unsigned int rd_mcycle(void)
{
    unsigned int v;
    __asm__ volatile ("csrr %0, mcycle" : "=r"(v));
    return v;
}

void uart_flush(void)
{
    unsigned int div, wait, t0;

    /* 1) Wait until the TX FIFO drains.
     *    With txmark(watermark)=1, ip.txwm = (txq.count < 1) = "FIFO empty". */
    UART_REG(UART_REG_TXCTRL) =
        UART_TXCTRL_TXEN | (1u << UART_TXCTRL_TXCNT_SHIFT);
    while (!(UART_REG(UART_REG_IP) & UART_IP_TXWM)) {
        /* busy wait */
    }

    /* 2) Even with an empty FIFO, one character is still going out through the
     *    shift register.
     *    1 character = start(1) + data(8) + stop(1) = 10 bits, 1 bit = div cycles.
     *    Wait for 4 character times to be safe. */
    div = UART_REG(UART_REG_DIV) & 0xFFFFu;
    if (div == 0) div = 4340u;          /* defensive: 500MHz/115200 */
    wait = div * 10u * 4u;
    t0 = rd_mcycle();
    while ((rd_mcycle() - t0) < wait) {
        /* busy wait */
    }
}
