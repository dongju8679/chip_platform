#include "rfm_init.h"
#include "rfm_intr.h"

WEAK int init_L1(void)
{
    return 0;
}

WEAK int init_RF(void)
{
    return 0;
}

int _user_init(void)
{
    init_L1();
    init_RF();
    intr_RegisterRfmSwInterrupts();
    return 0;
}

WEAK int halt_L1(void)
{
    return 0;
}

WEAK int halt_RF(void)
{
    return 0;
}

int mcu_sleep(void)
{
    halt_L1();
    halt_RF();
    return 0;
}
