/* bench_stdio.c - benchmark-only integer printf -> HTIF console (additive only)
 *
 * Supported: %c %s %d %i %u %x %X %o %p %% , field width / zero padding / left
 *            alignment, length modifiers l / ll / z
 * Unsupported: %f %e %g (no rv32 soft-float helpers - see the bench_stdio.h
 *            comment)
 */
#include "bench_stdio.h"
#include "bench_htif.h"

/* -- Output buffer -------------------------------------------
 * Each HTIF syscall round trip waits one fesvr polling period (on the order of
 * thousands of cycles on RTL). Accumulate into a buffer and send with a single
 * sys_write to cut the number of round trips.
 */
#define PBUF_SZ 256

typedef struct {
    char     buf[PBUF_SZ];
    unsigned n;
    int      count;
} pctx_t;

static void pflush(pctx_t *c)
{
    if (c->n) {
        htif_write(c->buf, c->n);
        c->n = 0;
    }
}

static void pemit(pctx_t *c, char ch)
{
    c->buf[c->n++] = ch;
    c->count++;
    if (c->n == PBUF_SZ) pflush(c);
}

static void pemit_pad(pctx_t *c, char ch, int n)
{
    while (n-- > 0) pemit(c, ch);
}

/* -- 64-bit division (shift-subtract) ------------------------
 * This is -nostdlib and there is no rv32 libgcc -> using u64 / u32 directly
 * yields undefined references to __udivdi3/__umoddi3. Handled with 32-bit
 * operations only.
 * (Same reason and same approach as Platform/Common/Src/printf.c)
 */
static unsigned long long div_u64_u32(unsigned long long n, unsigned int d,
                                      unsigned int *rem)
{
    unsigned long long q = 0;
    unsigned int r = 0;
    int i;

    for (i = 63; i >= 0; i--) {
        r = (r << 1) | (unsigned int)((n >> i) & 1u);
        if (r >= d) { r -= d; q |= (1ull << i); }
    }
    *rem = r;
    return q;
}

static int utoa_buf(char *out, unsigned long long v, unsigned base, int upper)
{
    static const char lo[] = "0123456789abcdef";
    static const char up[] = "0123456789ABCDEF";
    const char *digits = upper ? up : lo;
    char tmp[24];
    int i = 0, len;

    if (v == 0) {
        tmp[i++] = '0';
    } else if ((v >> 32) == 0) {
        unsigned int v32 = (unsigned int)v;      /* common path: HW 32-bit divu */
        while (v32) { tmp[i++] = digits[v32 % base]; v32 /= base; }
    } else {
        while (v) {
            unsigned int r;
            v = div_u64_u32(v, base, &r);
            tmp[i++] = digits[r];
        }
    }
    len = i;
    while (i--) *out++ = tmp[i];
    return len;
}

int vprintf(const char *fmt, va_list ap)
{
    pctx_t c;
    c.n = 0; c.count = 0;

    if (!fmt) return 0;

    while (*fmt) {
        int left = 0, zero = 0, width = 0, longs = 0, upper = 0, neg = 0;
        unsigned base = 10;
        unsigned long long uval = 0;
        char numbuf[24];
        int nlen, pad;

        if (*fmt != '%') { pemit(&c, *fmt++); continue; }
        fmt++;
        if (*fmt == '%') { pemit(&c, '%'); fmt++; continue; }

        for (;;) {
            if (*fmt == '-')      { left = 1; fmt++; }
            else if (*fmt == '0') { zero = 1; fmt++; }
            else break;
        }
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        /* Precision (.N) is parsed but ignored - it is not used in benchmark output */
        if (*fmt == '.') { fmt++; while (*fmt >= '0' && *fmt <= '9') fmt++; }
        while (*fmt == 'l') { longs++; fmt++; }
        if (*fmt == 'z') { longs = 1; fmt++; }

        switch (*fmt) {
        case 'c': {
            char ch = (char)va_arg(ap, int);
            if (!left) pemit_pad(&c, ' ', width - 1);
            pemit(&c, ch);
            if (left) pemit_pad(&c, ' ', width - 1);
            fmt++; continue;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            int len = 0;
            if (!s) s = "(null)";
            while (s[len]) len++;
            if (!left) pemit_pad(&c, ' ', width - len);
            { int i; for (i = 0; i < len; i++) pemit(&c, s[i]); }
            if (left) pemit_pad(&c, ' ', width - len);
            fmt++; continue;
        }
        case 'd': case 'i': {
            long long sv = (longs >= 2) ? va_arg(ap, long long)
                         : (longs == 1) ? (long long)va_arg(ap, long)
                                        : (long long)va_arg(ap, int);
            if (sv < 0) { neg = 1; uval = (unsigned long long)(-sv); }
            else uval = (unsigned long long)sv;
            break;
        }
        case 'u':
            uval = (longs >= 2) ? va_arg(ap, unsigned long long)
                 : (longs == 1) ? (unsigned long long)va_arg(ap, unsigned long)
                                : (unsigned long long)va_arg(ap, unsigned int);
            break;
        case 'X': upper = 1;  /* fallthrough */
        case 'x':
            base = 16;
            uval = (longs >= 2) ? va_arg(ap, unsigned long long)
                 : (longs == 1) ? (unsigned long long)va_arg(ap, unsigned long)
                                : (unsigned long long)va_arg(ap, unsigned int);
            break;
        case 'o':
            base = 8;
            uval = (longs >= 2) ? va_arg(ap, unsigned long long)
                 : (longs == 1) ? (unsigned long long)va_arg(ap, unsigned long)
                                : (unsigned long long)va_arg(ap, unsigned int);
            break;
        case 'p':
            base = 16;
            uval = (unsigned long long)(unsigned long)va_arg(ap, void *);
            pemit(&c, '0'); pemit(&c, 'x');
            break;
        default:
            pemit(&c, '%');
            if (*fmt) pemit(&c, *fmt++);
            continue;
        }
        fmt++;

        nlen = utoa_buf(numbuf, uval, base, upper);
        pad  = width - nlen - (neg ? 1 : 0);

        if (!left && !zero) pemit_pad(&c, ' ', pad);
        if (neg) pemit(&c, '-');
        if (!left && zero)  pemit_pad(&c, '0', pad);
        { int i; for (i = 0; i < nlen; i++) pemit(&c, numbuf[i]); }
        if (left) pemit_pad(&c, ' ', pad);
    }

    pflush(&c);
    return c.count;
}

int printf(const char *fmt, ...)
{
    va_list ap; int n;
    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

int ee_printf(const char *fmt, ...)
{
    va_list ap; int n;
    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

int debug_printf(const char *fmt, ...)
{
    va_list ap; int n;
    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

int putchar(int ch)
{
    htif_putc((char)ch);
    return ch;
}

int puts(const char *s)
{
    htif_puts(s);
    htif_putc('\n');
    return 0;
}
