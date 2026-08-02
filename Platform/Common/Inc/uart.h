#ifndef __UART__
#define __UART__

/* uart.h - chipyard SiFive UART0 driver (v301)
 *
 * DUT: RV32RocketConfig, serial@10020000, compatible "sifive,uart0".
 * This is a SiFive UART0, not an NS16550, so it uses txdata polling.
 *
 * Register map (per rocket-chip-blocks/devices/uart/UARTCtrlRegs.scala):
 *   0x00 txdata  [31] full(RO) / [7:0] character to write (WO)
 *   0x04 rxdata  [31] empty(RO) / [7:0] character read (RO)
 *   0x08 txctrl  [0] txen / [1] nstop
 *   0x0A txmark  txcnt watermark   <- separate register (sits at [23:16] of the
 *                                     0x08 word)
 *   0x0C rxctrl  [0] rxen
 *   0x0E rxmark  rxcnt watermark
 *   0x10 ie, 0x14 ip
 *   0x18 div     baud divisor = pbus freq / baudrate (the reset value already
 *                targets 115200)
 *
 * Caution 1: txen is 0 at reset (UART.scala: val txen = RegInit(false.B)).
 *         With txen=0 the TX FIFO never drains, so it hangs with the full bit
 *         stuck.
 *         -> Always call uart_init() first.
 *
 * Caution 2: always call uart_flush() before ending the simulation (tohost).
 *         The moment exit() writes tohost the host terminates the simulation
 *         immediately, so characters still in the TX FIFO / shift register
 *         never reach stdout and are truncated.
 */

#include <stdint.h>

/* Makefile.VERIF extracts this from the DTS automatically and passes it as
 * -DUART_BASE_ADDR=0x... */
#ifndef UART_BASE_ADDR
#define UART_BASE_ADDR 0x10020000
#endif

#define UART_REG_TXDATA 0x00
#define UART_REG_RXDATA 0x04
#define UART_REG_TXCTRL 0x08
#define UART_REG_RXCTRL 0x0C
#define UART_REG_IE     0x10
#define UART_REG_IP     0x14
#define UART_REG_DIV    0x18

#define UART_TXDATA_FULL  (1u << 31)
#define UART_RXDATA_EMPTY (1u << 31)
#define UART_TXCTRL_TXEN  (1u << 0)
#define UART_RXCTRL_RXEN  (1u << 0)
/* Position of txmark (0x0A) within the 32-bit txctrl word */
#define UART_TXCTRL_TXCNT_SHIFT 16
#define UART_IP_TXWM      (1u << 0)   /* ip.txwm = (txq.count < txmark) */

#define UART_REG(off) (*(volatile unsigned int *)((unsigned int)UART_BASE_ADDR + (off)))

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_write(const char *buf, unsigned int len);
void uart_flush(void);   /* wait for TX to drain completely before exit */

#endif /* __UART__ */
