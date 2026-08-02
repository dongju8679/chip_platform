#include "../common/hal.h"

int test_main(void) {
    volatile int *ptr = (volatile int*)0x80002000;
    int i;

    for (i = 0; i < 10; i++) {
        ptr[i] = i * 0xAA;
    }

    for (i = 0; i < 10; i++) {
        if (ptr[i] != i * 0xAA) {
            return i + 1;
        }
    }

    uart_puts("Memory test PASSED\n");
    return 0;
}
