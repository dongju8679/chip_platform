#include "type.h"
#include "util.h"
#include "interrupt.h"
#include "CLINT_timer_interrupt.h"
#include "timer.h"

int timer_test(void)
{
    TEST++;
    return 0;
}

void CLINT_timer_interrupt()
{
    uint32_t bp_num;
    init_timeint_handler();
    TEST = 0;
    GLOBAL_INTERRUPT_DISABLE();    
    for (bp_num = 0; bp_num < BP_MAX; bp_num++) {
        register_timer(timer_test, 100, 0);
        if (dut_wait_bp(BP_TEST_begin))
            exit(CODE_FAIL);
        
        GLOBAL_INTERRUPT_ENABLE();
        for (volatile int i = 0; i < 50000; i++);
        GLOBAL_INTERRUPT_DISABLE();
        dut_set_bp(BP_TEST_end);
        
        if (dut_wait_bp(BP_TEST_success))
            exit(CODE_FAIL);
    }
}

int main()
{
    CLINT_timer_interrupt();
    return CODE_SUCCESS;
}
