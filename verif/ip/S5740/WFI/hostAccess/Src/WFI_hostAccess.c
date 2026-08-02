#include "type.h"
#include "util.h"
#include "interrupt.h"
#include "WFI_hostAccess.h"

int ISR(){
    return 0;
}

void WFI_hostAccess()
{
    init_extint_handler();
    register_extint_handler(1, ISR, 1);
    write_csr(mie, MEIE);
    GLOBAL_INTERRUPT_ENABLE();

    asm volatile("wfi");
    GLOBAL_INTERRUPT_DISABLE();
}

int main()
{
    WFI_hostAccess();
    return CODE_SUCCESS;
}

