/* data_init.c - .data initialization / .bss clearing
 *
 * See Platform/Common/Inc/data_init.h for the design rationale (why the .data
 * copy is a no-op today, and why crt.S was left alone).
 */
#include "data_init.h"

/* The .bss boundaries provided by link.ld. The address itself is the value. */
extern char __bss_start[];
extern char __bss_end[];

/* .data relocation symbols. The current link.ld does not create them.
 * Declaring them weak makes their address 0 when absent, so compilation and
 * linking do not break.
 * When moving to a ROM boot, PROVIDE the three symbols below in link.ld and the
 * copy turns on automatically:
 *   __data_load_start = LOADADDR(.data);
 *   __data_start = ADDR(.data);
 *   __data_end   = ADDR(.data) + SIZEOF(.data);
 */
extern char __data_load_start[] __attribute__((weak));
extern char __data_start[]      __attribute__((weak));
extern char __data_end[]        __attribute__((weak));

void data_init_bss_range(xlen_t *start, xlen_t *end)
{
    if (start) *start = (xlen_t)(unsigned long)__bss_start;
    if (end)   *end   = (xlen_t)(unsigned long)__bss_end;
}

void data_init_bss(void)
{
    /* Same approach as crt.S (4 bytes at a time). link.ld aligns __bss_end to
     * ALIGN(4), so no tail handling is needed. */
    volatile unsigned int *p   = (volatile unsigned int *)__bss_start;
    volatile unsigned int *end = (volatile unsigned int *)__bss_end;
    while (p < end) *p++ = 0u;
}

void data_init_data(void)
{
    char *src, *dst, *end;

    /* If the symbols are absent (= the current link.ld) their address is 0 ->
     * nothing to do. There is also nothing to copy when LMA equals VMA. */
    if (!__data_load_start || !__data_start || !__data_end) return;
    /* Comparing arrays directly triggers -Warray-compare. Compare addresses. */
    if (&__data_load_start[0] == &__data_start[0]) return;

    src = __data_load_start;
    dst = __data_start;
    end = __data_end;
    while (dst < end) *dst++ = *src++;
}

void data_init(void)
{
    data_init_data();
    data_init_bss();
}

int data_init_bss_is_zero(void)
{
    volatile unsigned int *p   = (volatile unsigned int *)__bss_start;
    volatile unsigned int *end = (volatile unsigned int *)__bss_end;
    while (p < end) {
        if (*p != 0u) return 0;
        p++;
    }
    return 1;
}
