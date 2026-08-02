#include "exceptions.h"
#include "interrupt.h"
#include "chip.h"
#include "ipc.h"

void exit(unsigned int code)
{
    flush_ipc();
    alert_master();
    outpdw(EXIT_CODE, code);
    enable_debug_interrupt();

    SET_PLIC_THRESHOLD(0);
    CLEAR_INTERRUPT_ENABLE(_ALL_IE_BITS);
    SET_INTERRUPT_ENABLE(MEIE);

    GLOBAL_INTERRUPT_ENABLE();

    while(1);
}

void alert_master()
{
    outpdw(GPIO_IRQ_ADDR, 1 | inpdw(GPIO_IRQ_ADDR));
}

void enable_debug_interrupt()
{
    DISABLE_ALL_PLIC_INT();
    SET_PLIC_INT_PRIORITY(IPC_INTERRUPT, ipc_priority);
    ENABLE_PLIC_INT(IPC_INTERRUPT);
}

int user_exception_handler()
{
    pEXIT_INFO->mcause = 0;
    pEXIT_INFO->mepc = 0;
    pEXIT_INFO->ecode = CODE_USER_DEFINED;
    pEXIT_INFO->exit_cycles = (read_csr(mcycle) >> 8) | (read_csr(mcycle) << 24);
    exit(CODE_USER_DEFINED);
    return CODE_FATAL;
}

int exception_handler(xlen_t cause)
{
    pEXIT_INFO->mcause = (cause > 0xf) ? 0xf:cause;
    pEXIT_INFO->exit_cycles = (read_csr(mcycle) >> 8) | (read_csr(mcycle) << 24);
    exit(CODE_FATAL);
    return CODE_FATAL;
}
