/* bench_libc.c - minimal implementations of the standard C library functions
 *                the benchmarks require (additive only)
 *
 * Why implement them ourselves (what the investigation showed)
 *   $ riscv64-unknown-elf-gcc -print-multi-lib
 *     .;                                   <- only the default multilib
 *   $ riscv64-unknown-elf-gcc -march=rv32imac_zicsr_zifencei -mabi=ilp32 \
 *         -print-file-name=libc.a
 *     .../riscv64-unknown-elf/lib/libc.a   <- a path is returned, but it is
 *                                             ELF64 / lp64d
 *   $ riscv64-unknown-elf-readelf -h .../libc.a | head
 *     Class: ELF64,  Flags: RVC, double-float ABI
 *   $ riscv64-unknown-elf-gcc -march=rv32imac_zicsr_zifencei -mabi=ilp32 t.c
 *     fatal error: Cannot find suitable multilib set for
 *                  '-march=rv32imac_zicsr_zifencei'/'-mabi=ilp32'
 *   -> This chipyard conda toolchain has neither rv32 newlib (libc.a) nor rv32
 *      libgcc. The headers (<string.h> etc.) are in the sysroot, so the
 *      declarations resolve fine.
 *
 *   Alternatives considered
 *     (a) rebuild newlib-nano/picolibc for rv32 -> 30-60 min + a polluted
 *         toolchain
 *     (b) implement only the functions the benchmarks actually call -> the 8
 *         below, a few dozen lines
 *   The libc CoreMark and Dhrystone use inside their measurement loops is about
 *   memcpy/memcmp/strcpy/strcmp, and malloc can be avoided with MEM_STATIC.
 *   (b) is overwhelmingly cheaper, so (b) was chosen.
 *
 * A note on measurement fairness
 *   Dhrystone calls strcpy/strcmp *inside* the measurement loop, so the quality
 *   of the implementations in this file feeds directly into the score. Compared
 *   with newlib's word-optimized strcpy, the byte loops below are at a
 *   disadvantage. This fact is stated explicitly in the results document, and
 *   this file is built with -O2 alongside everything else so the compiler can
 *   apply its own optimizations.
 *   (Meaning: the number reflects "this software stack's" performance, not the
 *    hardware's.)
 *
 * Even under -ffreestanding, GCC synthesizes memcpy/memset for struct
 * assignments and array initialization. Those calls land here too.
 */

#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    /* If both sides are aligned, copy word by word (most struct assignments
     * land here) */
    if ((((unsigned long)d | (unsigned long)s) & 3u) == 0) {
        unsigned long       *dw = (unsigned long *)d;
        const unsigned long *sw = (const unsigned long *)s;
        while (n >= 4) { *dw++ = *sw++; n -= 4; }
        d = (unsigned char *)dw;
        s = (const unsigned char *)sw;
    }
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    if (d == s || n == 0) return dst;
    if (d < s) return memcpy(dst, src, n);

    d += n; s += n;
    while (n--) *--d = *--s;
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    unsigned char  v = (unsigned char)c;

    if ((((unsigned long)d) & 3u) == 0 && n >= 4) {
        unsigned long  w  = ((unsigned long)v << 24) | ((unsigned long)v << 16)
                          | ((unsigned long)v << 8)  |  (unsigned long)v;
        unsigned long *dw = (unsigned long *)d;
        while (n >= 4) { *dw++ = w; n -= 4; }
        d = (unsigned char *)dw;
    }
    while (n--) *d++ = v;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = (const unsigned char *)a;
    const unsigned char *q = (const unsigned char *)b;

    while (n--) {
        if (*p != *q) return (int)*p - (int)*q;
        p++; q++;
    }
    return 0;
}

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++) != '\0') { }
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    char *d = dst;
    while (n && (*d = *src) != '\0') { d++; src++; n--; }
    while (n--) *d++ = '\0';
    return dst;
}

int strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char *strcat(char *dst, const char *src)
{
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++) != '\0') { }
    return dst;
}
