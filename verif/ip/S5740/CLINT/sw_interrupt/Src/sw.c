#include "sw.h"
#include "interrupt.h"

void init_swint_handler(void)
{
    pISR_BASE.sw_int_handler = empty;
    SET_INTERRUPT_ENABLE(MSIE);
}

int register_swint_handler(int (*handler)(void))
{
    pISR_BASE.sw_int_handler = handler;
    return 0;
}
