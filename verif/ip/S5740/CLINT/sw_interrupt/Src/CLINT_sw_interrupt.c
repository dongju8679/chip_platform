#include "type.h"
#include "util.h"
#include "interrupt.h"
#include "CLINT_sw_interrupt.h"
#include "sw.h"

int swint_test(void)
{
    TEST++;
    outpdw(0x02000000, 0);
    return 0;
}

void CLINT_sw_interrupt()
{
    init_swint_handler();
    register_swint_handler(swint_test);
    uint32_t bp_num;
    for (bp_num = 0; bp_num < BP_MAX; bp_num++) {
        GLOBAL_INTERRUPT_DISABLE();
        if (dut_wait_bp(BP_TEST_begin))
            exit(CODE_FAIL);
        GLOBAL_INTERRUPT_ENABLE();
        dut_set_bp(BP_TEST_end);
        if (dut_wait_bp(BP_TEST_success))
            exit(CODE_FAIL);
    }
}

int main()
{
    CLINT_sw_interrupt();
    return CODE_SUCCESS;
}
