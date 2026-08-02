#include "type.h"
#include "util.h"
#include "interrupt.h"
#include "PLIC_enable.h"

int test(void)
{
    TEST++;
    return 0;
}

void PLIC_enable()
{
    init_extint_handler();
    GLOBAL_INTERRUPT_DISABLE();
    TEST = 0;
    uint32_t bp_num;
    for (bp_num = 1; bp_num < MAX_PLIC_NUMBER + 1; bp_num++) {
        register_extint_handler(bp_num, test, 1);
        if (dut_wait_bp(BP_TEST_begin))
            exit(CODE_FAIL);

        GLOBAL_INTERRUPT_ENABLE();
        GLOBAL_INTERRUPT_DISABLE();
        dut_set_bp(BP_TEST_end);

        if (dut_wait_bp(BP_TEST_success))
            exit(CODE_FAIL);
    }
}

#define ULL unsigned long long

void PLIC_disable()
{
    dut_set_bp(BP_TEST_init);
    init_extint_handler();
    GLOBAL_INTERRUPT_DISABLE();
    TEST = 0;
    uint32_t bp_num;
    for (bp_num = 1; bp_num < MAX_PLIC_NUMBER + 1; bp_num++) {
        register_extint_handler(bp_num, test, 1);
        DISABLE_PLIC_INT(bp_num);
        if (dut_wait_bp(BP_TEST_begin))
            exit(CODE_FAIL);
        GLOBAL_INTERRUPT_ENABLE();
        GLOBAL_INTERRUPT_DISABLE();
        dut_set_bp(BP_TEST_end);
        if (dut_wait_bp(BP_TEST_success))
            exit(CODE_FAIL);        
    }
}

int main()
{
    PLIC_enable();
    PLIC_disable();
    return CODE_SUCCESS;
}