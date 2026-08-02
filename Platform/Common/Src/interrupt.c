#include "util.h"
#include "interrupt.h"

ISR_GROUP pISR_BASE;

int empty(void)
{
return 0;
}

int _register_extint_handler(int num, int(*handler)())
{
pISR_BASE.ext_int_handler[num] = handler;
return 0;
}

int register_extint_handler(int num, int (*handler)(), enum _priority pri)
{
if (num > PLIC_NUMBER_OF_INTERRUPTS || num < 0) return CODE_FAIL;
if(pri < 0 || pri>PLIC_MAX_PRIORITY) return CODE_FAIL;

ENABLE_PLIC_INT(num);
_register_extint_handler(num, handler);
SET_PLIC_INT_PRIORITY(num, pri);
return CODE_SUCCESS;
}

void init_extint_handler(void)
{
DISABLE_ALL_PLIC_INT();
for(unsigned int i = 1; i <= PLIC_NUMBER_OF_INTERRUPTS; i++)
{
pISR_BASE.ext_int_handler[i] = empty;
SET_PLIC_INT_PRIORITY(i,0);
}
SET_PLIC_THRESHOLD(0);
CLEAR_INTERRUPT_ENABLE(_ALL_IE_BITS);
SET_INTERRUPT_ENABLE(MEIE);
}