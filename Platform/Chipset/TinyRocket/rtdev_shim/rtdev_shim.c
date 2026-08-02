/* rtdev_shim.c - the minimal shim for the rt-dev track only (v301)
 *
 * Replaces the v294 hal_shim.c. In the rt-dev approach:
 *   - inpdw/outpdw are util.h macros (direct memory access: CLINT/mailbox)
 *     -> not defined here (the macros handle it)
 *   - exit = tohost (riscv-tests convention)
 *
 * hal_shim.c's complex address dispatch (rtc mock, PMU mock, shadow) is
 * unnecessary for sw_interrupt (it accesses CLINT 0x02000000 and mailbox
 * 0x80010000 directly).
 */
#include <stdint.h>

/* riscv-tests convention. Defined by BSP link.ld. */
extern volatile uint64_t tohost;

#define CODE_SUCCESS 0

void exit(unsigned int code)
{
    tohost = (code == CODE_SUCCESS) ? (uint64_t)1
                                    : (((uint64_t)code << 1) | 1u);
    for (;;) { }
}
