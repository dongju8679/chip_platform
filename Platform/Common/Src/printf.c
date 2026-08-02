/* printf.c - baremetal printf (v301)
 *
 * All output funnels into _write(1, buf, len) (the same lower interface newlib
 * uses). _write() is wired to uart_putc in Platform/Common/Src/syscalls.c.
 * See the Platform/Common/Inc/printf.h comment for why this is implemented
 * in-house (no rv32 newlib).
 */
#include "printf.h"

/* -- Output buffer: reduces per-character _write calls ------------ */
#define PBUF_SZ 128

typedef struct {
    char     buf[PBUF_SZ];
    unsigned n;
    int      count;   /* total characters emitted (printf return value) */
} pctx_t;

static void pflush(pctx_t *c)
{
    if (c->n) {
        _write(1, c->buf, (int)c->n);
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

/* -- 64-bit / 32-bit division (shift-subtract) ----------------
 * This is a -nostdlib build, so libgcc is not linked. On top of that, this
 * toolchain has no libgcc for rv32imac/ilp32 at all (multilib = "." only).
 * -> Using u64 % / u64 / directly yields undefined references to
 *    __umoddi3/__udivdi3.
 * Replaced with a bitwise long division that uses only 32-bit operations.
 */
static unsigned long long div_u64_u32(unsigned long long n, unsigned int d,
                                      unsigned int *rem)
{
    unsigned long long q = 0;
    unsigned int r = 0;
    int i;

    for (i = 63; i >= 0; i--) {
        r = (r << 1) | (unsigned int)((n >> i) & 1u);
        if (r >= d) {
            r -= d;
            q |= (1ull << i);
        }
    }
    *rem = r;
    return q;
}

/* -- Integer -> string (base 8/10/16) ------------------------- */
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
        /* Common case: an XLEN=32 value -> hardware M-extension 32-bit divide */
        unsigned int v32 = (unsigned int)v;
        while (v32) {
            tmp[i++] = digits[v32 % base];
            v32 /= base;
        }
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
    char numbuf[24];

    c.n = 0;
    c.count = 0;
    if (!fmt) return 0;

    while (*fmt) {
        int left = 0, zero = 0, plus = 0, space = 0, alt = 0;
        int width = 0, lcount = 0;
        int numlen, padlen;
        int neg = 0;
        char prefix[3];
        int preflen = 0;
        unsigned base = 10, upper = 0;
        unsigned long long uval = 0;
        const char *sval = 0;
        char cval;

        if (*fmt != '%') { pemit(&c, *fmt++); continue; }
        fmt++;

        /* flags */
        for (;;) {
            if      (*fmt == '-') { left  = 1; fmt++; }
            else if (*fmt == '0') { zero  = 1; fmt++; }
            else if (*fmt == '+') { plus  = 1; fmt++; }
            else if (*fmt == ' ') { space = 1; fmt++; }
            else if (*fmt == '#') { alt   = 1; fmt++; }
            else break;
        }
        /* width */
        if (*fmt == '*') {
            width = va_arg(ap, int);
            if (width < 0) { left = 1; width = -width; }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        }
        /* precision: parsed but ignored (minimal implementation for baremetal logging) */
        if (*fmt == '.') {
            fmt++;
            if (*fmt == '*') { (void)va_arg(ap, int); fmt++; }
            else while (*fmt >= '0' && *fmt <= '9') fmt++;
        }
        /* length */
        while (*fmt == 'l') { lcount++; fmt++; }
        while (*fmt == 'h' || *fmt == 'z') { fmt++; }

        switch (*fmt) {
        case '\0':
            pemit(&c, '%');
            continue;

        case '%':
            fmt++;
            pemit(&c, '%');
            continue;

        case 'c':
            fmt++;
            cval = (char)va_arg(ap, int);
            if (!left) pemit_pad(&c, ' ', width - 1);
            pemit(&c, cval);
            if (left)  pemit_pad(&c, ' ', width - 1);
            continue;

        case 's': {
            int slen = 0;
            fmt++;
            sval = va_arg(ap, const char *);
            if (!sval) sval = "(null)";
            while (sval[slen]) slen++;
            if (!left) pemit_pad(&c, ' ', width - slen);
            { int i; for (i = 0; i < slen; i++) pemit(&c, sval[i]); }
            if (left)  pemit_pad(&c, ' ', width - slen);
            continue;
        }

        case 'd':
        case 'i': {
            long long sv = (lcount >= 2) ? va_arg(ap, long long)
                         : (lcount == 1) ? (long long)va_arg(ap, long)
                                         : (long long)va_arg(ap, int);
            if (sv < 0) { neg = 1; uval = (unsigned long long)(-sv); }
            else        { uval = (unsigned long long)sv; }
            base = 10;
            fmt++;
            break;
        }

        case 'u':
            uval = (lcount >= 2) ? va_arg(ap, unsigned long long)
                 : (lcount == 1) ? (unsigned long long)va_arg(ap, unsigned long)
                                 : (unsigned long long)va_arg(ap, unsigned int);
            base = 10;
            fmt++;
            break;

        case 'o':
            uval = (lcount >= 2) ? va_arg(ap, unsigned long long)
                 : (lcount == 1) ? (unsigned long long)va_arg(ap, unsigned long)
                                 : (unsigned long long)va_arg(ap, unsigned int);
            base = 8;
            fmt++;
            break;

        case 'X':
            upper = 1;
            /* fallthrough */
        case 'x':
            uval = (lcount >= 2) ? va_arg(ap, unsigned long long)
                 : (lcount == 1) ? (unsigned long long)va_arg(ap, unsigned long)
                                 : (unsigned long long)va_arg(ap, unsigned int);
            base = 16;
            if (alt && uval) { prefix[preflen++] = '0'; prefix[preflen++] = upper ? 'X' : 'x'; }
            fmt++;
            break;

        case 'p':
            uval = (unsigned long long)(unsigned long)va_arg(ap, void *);
            base = 16;
            prefix[preflen++] = '0';
            prefix[preflen++] = 'x';
            fmt++;
            break;

        default:
            /* Emit an unknown conversion specifier verbatim */
            pemit(&c, '%');
            pemit(&c, *fmt++);
            continue;
        }

        if (neg)        prefix[preflen++] = '-';
        else if (plus)  prefix[preflen++] = '+';
        else if (space) prefix[preflen++] = ' ';

        numlen = utoa_buf(numbuf, uval, base, upper);
        padlen = width - numlen - preflen;

        if (!left && !zero) pemit_pad(&c, ' ', padlen);
        { int i; for (i = 0; i < preflen; i++) pemit(&c, prefix[i]); }
        if (!left && zero)  pemit_pad(&c, '0', padlen);
        { int i; for (i = 0; i < numlen; i++) pemit(&c, numbuf[i]); }
        if (left)           pemit_pad(&c, ' ', padlen);
    }

    pflush(&c);
    return c.count;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

int putchar(int ch)
{
    char c = (char)ch;
    _write(1, &c, 1);
    return ch;
}

int puts(const char *s)
{
    int len = 0;
    if (!s) s = "(null)";
    while (s[len]) len++;
    _write(1, s, len);
    _write(1, "\n", 1);
    return len + 1;
}
