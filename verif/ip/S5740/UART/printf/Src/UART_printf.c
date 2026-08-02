/* UART_printf.c - chipyard UART0 stdout verification (v301)
 *
 * Goal: the DUT prints on its own via printf (not observed through a host
 * master_read).
 *
 * Path:  printf()  [Platform/Common/Src/printf.c]
 *          -> _write(1, buf, len)  [Platform/Common/Src/syscalls.c]
 *          -> uart_write/uart_putc [Platform/Common/Src/uart.c]
 *          -> SiFive UART0 @0x10020000 txdata (polling bit31 full)
 *          -> harness UARTAdapter/SimUART -> simulator stdout
 *
 * Run: simulator-chipyard.harness-RV32RocketConfig build/UART_printf.elf
 *       (do not pass +verif= -> uses the standard testchip_tsi_t)
 */
#include "type.h"
#include "util.h"
#include "uart.h"
#include "printf.h"
#include "UART_printf.h"

int main(void)
{
    /* txen is 0 at reset, so it must be enabled first (see the uart.h comment) */
    uart_init();

    /* 1) Check the lowest-level driver path */
    uart_puts("[uart_puts] UART0 alive\n");

    /* 2) The actual subject under test: printf */
    printf(UART_HELLO_MSG);

    /* 3) Check the format conversion path */
    printf("[printf] dec=%d hex=0x%08x str=%s char=%c\n",
           -12345, 0xDEADBEEFu, "chipyard", 'K');
    printf("[printf] pad |%8d|%-8d|%05u| ptr=%p\n",
           42, 42, 7u, (void *)0x80000000u);
    printf("[printf] mcycle=%u\n", (unsigned int)read_csr(mcycle));

#if defined(USE_NEWLIB_STDIO) && USE_NEWLIB_STDIO
    /* 3b) Soft-float conversion path (added after the newlib stdio switch)
     *   rv32imac has no F extension -> %f/%e/%g are all handled in soft float
     *   (__adddf3/__divdf3/__gdtoa). Any FPU instruction would raise an illegal
     *   instruction trap, so this line printing correctly is itself the proof.
     *   newlib-nano excludes float conversion by default, so
     *   TEST_PRINTF_FLOAT=1 in test.mk adds -u _printf_float.
     *   Expected output: [printf] f=3.141590 e=2.500000e-03 g=0.333333 */
    printf("[printf] f=%f e=%e g=%g\n", 3.14159, 2.5e-3, 1.0 / 3.0);
#endif

    /* 4) Completion marker: if this line appears, the whole stdout path is alive */
    printf("UART_PRINTF_DONE\n");

    /* 5) The drain wait is mandatory. When main returns, crt.S calls exit()
     *    which writes tohost, and the host terminates the simulation
     *    immediately, truncating whatever is left in the FIFO.
     *    (Running without the flush really did cut off after "UART_PRI".) */
    uart_flush();

    return CODE_SUCCESS;
}
