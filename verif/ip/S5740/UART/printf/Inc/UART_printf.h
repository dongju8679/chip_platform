#ifndef __UART_PRINTF__
#define __UART_PRINTF__

/* UART_printf - DUT-driven stdout verification case (v301)
 *
 * Unlike the existing IP verification cases, there is no host (Vseq)
 * interaction. The DUT pushes characters directly to chipyard UART0
 * (0x10020000), and the harness's UARTAdapter/SimUART prints them to the
 * simulator's stdout.
 * -> Run with the standard SimTSI, without +verif=.
 */

#define UART_HELLO_MSG "Hello chipyard baremetal\n"

#endif
