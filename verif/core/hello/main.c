#include "../common/hal.h"

int test_main(void) {
    uart_puts("Hello from TinyRocketConfig!\n");
    uart_puts("chip_platform baremetal test\n");
    return 0;
}
