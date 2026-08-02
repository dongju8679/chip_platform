/* newlib_glue.c - the minimal glue needed to run newlib stdio on this
 *                 heap-less bare-metal target
 *
 * Only compiled in when USE_NEWLIB_STDIO=1 (Implementation/Makefile.VERIF).
 * With USE_NEWLIB_STDIO=0 this file is not linked and the original printf.c is
 * used instead.
 *
 * -- Why this file exists ------------------------------------------
 * Dropping -nostdlib causes libgloss to be linked automatically. But libgloss's
 * _sbrk/_fstat/_isatty/_lseek/_close/_read are the proxy-kernel ABI versions and
 * all issue an `ecall`:
 *
 *     _sbrk:  li a7,214 ; ecall        # SYS_brk
 *
 * This DUT is M-mode only with no proxy kernel. An M-mode ecall traps with
 * mcause=11 and trap_handler in syscalls.c calls exit(CODE_FATAL). Left as is,
 * it would die on the very first printf. So the same names are defined here to
 * displace the libgloss versions (an object file wins over an archive).
 *
 * -- Heap policy ---------------------------------------------------
 * malloc is not opened up in general. The heap here is "the fixed region printf
 * minimally requires"; beyond that _sbrk returns -1 (ENOMEM).
 *
 * Measured (on spike, recovered through the exit-code channel - see chapter 6
 * of docs/newlib_transition.md):
 *   - integer conversions only (%d %x %s %c %p) + setvbuf(_IONBF)
 *       -> heap 0 B, _sbrk called 0 times
 *   - adding %f %e %g + setvbuf(_IONBF)
 *       -> heap 232 B, _sbrk called 5 times
 *     (the amount used by __gdtoa's Bigint under _printf_float)
 * So 232 B is the real requirement, and NEWLIB_HEAP_SIZE is set to 512 B for
 * headroom. Out of a 60K (61,440 B) budget, 512 B is negligible, and exceeding
 * it merely makes _sbrk return -1 rather than writing past memory (newlib
 * handles malloc failure gracefully).
 *
 * -- Why setvbuf(_IONBF) is needed ---------------------------------
 * Without it, newlib mallocs the stdout FILE buffer (1 KB) on the first output:
 *   printf -> _vfiprintf_r -> __swsetup_r -> __smakebuf_r -> _malloc_r -> _sbrk
 * Measured at 1,032 B. Unbuffered, it uses the 1-byte _nbuf inside FILE, so that
 * whole 1 KB disappears. The output path is character-by-character UART anyway
 * (~43k cycles per character), so buffering gains nothing.
 * The call site is set_log() in util.c - the hook crt.S invokes just before main
 * (see the Platform/Common/Inc/boot.h comment). crt.S was not touched.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef NEWLIB_HEAP_SIZE
#define NEWLIB_HEAP_SIZE 512
#endif

/* -- Fixed heap ---------------------------------------------------
 * Instead of using link.ld's __heap_start~__stack_top, the heap is an array in
 * .bss. That way its size shows up directly in the image size (the `size`
 * output) at link time, so the linker catches a 60K budget overrun immediately.
 * It also cannot possibly overlap the stack. */
static char     nl_heap[NEWLIB_HEAP_SIZE];
static unsigned nl_brk;

void *_sbrk(int incr)
{
    if (incr < 0) return (void *)-1;
    if (nl_brk + (unsigned)incr > (unsigned)NEWLIB_HEAP_SIZE)
        return (void *)-1;                 /* ENOMEM - newlib handles this gracefully */
    void *p = &nl_heap[nl_brk];
    nl_brk += (unsigned)incr;
    return p;
}

/* -- Minimal definitions that displace the libgloss ecall versions ----
 * With setvbuf(_IONBF) in effect, the functions below are never called on the
 * execution path (measured syscall mask: 0). They are still defined because the
 * libgloss versions carry an ecall inside them even when merely linked, so the
 * moment anyone later uses fopen/fseek and friends it would go straight to
 * FATAL. These definitions fail quietly without issuing an ecall. */
int _fstat(int fd, struct stat *st)
{
    if (!st) return -1;
    st->st_mode = S_IFCHR;                 /* character device (UART) */
    return (fd >= 0 && fd <= 2) ? 0 : -1;
}

int _isatty(int fd)          { return (fd >= 0 && fd <= 2); }
off_t _lseek(int fd, off_t off, int whence) { (void)fd; (void)off; (void)whence; return 0; }
int _close(int fd)           { (void)fd; return -1; }
int _read(int fd, char *buf, int len) { (void)fd; (void)buf; (void)len; return 0; }

/* -- Make stdout unbuffered ---------------------------------------
 * set_log() in util.c calls this once, just before main. */
void newlib_stdio_init(void)
{
    static int done;
    if (done) return;
    done = 1;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
}
