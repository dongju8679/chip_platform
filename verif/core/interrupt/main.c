#include "../common/hal.h"

volatile int irq_fired = 0;

void __attribute__((interrupt("machine"), aligned(4))) my_handler(void) {
    /* disable MSIE first (prevent re-entry) */
    uint32_t val = 0x8;
    asm volatile("csrc mie, %0" :: "r"(val));

    /* clear MSIP */
    CLINT_MSIP = 0;
    asm volatile("fence");

    irq_fired = 1;
}

int test_main(void) {
    uint32_t val;
    uintptr_t addr = (uintptr_t)my_handler & ~0x3UL;

    asm volatile("csrw mtvec, %0" :: "r"(addr));
    asm volatile("csrsi mstatus, 0x8");
    val = 0x8;
    asm volatile("csrs mie, %0" :: "r"(val));

    CLINT_MSIP = 1;

    asm volatile("nop; nop; nop; nop; nop");

    if (!irq_fired) {
        uart_puts("Interrupt FAILED\n");
        return 1;
    }

    uart_puts("Interrupt PASSED\n");
    return 0;
}
