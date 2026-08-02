/* core_portme.h - the CoreMark porting layer (chip_platform / RV32RocketConfig)
 *
 * Based on the official EEMBC barebones/core_portme.h, filled in for this
 * target. The CoreMark body (core_main.c / core_list_join.c / core_matrix.c /
 * core_state.c / core_util.c / coremark.h) is upstream and unmodified
 * (Middleware/benchmark/UPSTREAM.md5).
 *
 * Target decisions and their rationale
 *  - HAS_FLOAT 0   : rv32imac has no F extension and this toolchain has no rv32
 *                    libgcc, so the soft-float helpers (__adddf3/__divdf3...)
 *                    cannot be linked.
 *                    -> Disable CoreMark's own float score printing; the score
 *                       is computed in integer arithmetic from mcycle by the
 *                       porting layer.
 *  - HAS_STDIO 0 / HAS_PRINTF 0 : no newlib. ee_printf is wired to
 *                    bench_stdio.c.
 *  - MEM_METHOD MEM_STATIC : no malloc (no newlib). TOTAL_DATA_SIZE=2000 is
 *                    allocated as a static array in .bss. The linker RAM is
 *                    60K, so there is plenty of room.
 *  - SEED_METHOD SEED_VOLATILE : bare metal with no command line arguments.
 *  - CORETIMETYPE ee_u32 : the low 32 bits of mcycle. See core_portme.c for the
 *                    overflow analysis.
 */
#ifndef CORE_PORTME_H
#define CORE_PORTME_H

#include <stddef.h>

#define HAS_FLOAT   0
#define HAS_TIME_H  0
#define USE_CLOCK   0
#define HAS_STDIO   0
#define HAS_PRINTF  0

#ifndef COMPILER_VERSION
#define COMPILER_VERSION "GCC" __VERSION__
#endif
#ifndef COMPILER_FLAGS
#define COMPILER_FLAGS FLAGS_STR
#endif
#ifndef MEM_LOCATION
#define MEM_LOCATION "STATIC"
#endif

typedef signed short   ee_s16;
typedef unsigned short ee_u16;
typedef signed int     ee_s32;
typedef double         ee_f32;
typedef unsigned char  ee_u8;
typedef unsigned int   ee_u32;
typedef ee_u32         ee_ptr_int;   /* ilp32: pointer = 32-bit */
typedef size_t         ee_size_t;
#ifndef NULL
#define NULL ((void *)0)
#endif

#define align_mem(x) (void *)(4 + (((ee_ptr_int)(x)-1) & ~3))

#define CORETIMETYPE ee_u32
typedef ee_u32 CORE_TICKS;

#ifndef SEED_METHOD
#define SEED_METHOD SEED_VOLATILE
#endif

#ifndef MEM_METHOD
#define MEM_METHOD MEM_STATIC
#endif

#ifndef MULTITHREAD
#define MULTITHREAD 1
#define USE_PTHREAD 0
#define USE_FORK    0
#define USE_SOCKET  0
#endif

/* crt.S calls it with `jal main`, without arguments */
#ifndef MAIN_HAS_NOARGC
#define MAIN_HAS_NOARGC 1
#endif
#ifndef MAIN_HAS_NORETURN
#define MAIN_HAS_NORETURN 0
#endif

extern ee_u32 default_num_contexts;

typedef struct CORE_PORTABLE_S
{
    ee_u8 portable_id;
} core_portable;

void portable_init(core_portable *p, int *argc, char *argv[]);
void portable_fini(core_portable *p);

#if !defined(PROFILE_RUN) && !defined(PERFORMANCE_RUN) \
    && !defined(VALIDATION_RUN)
#if (TOTAL_DATA_SIZE == 1200)
#define PROFILE_RUN 1
#elif (TOTAL_DATA_SIZE == 2000)
#define PERFORMANCE_RUN 1
#else
#define VALIDATION_RUN 1
#endif
#endif

int ee_printf(const char *fmt, ...);

#endif /* CORE_PORTME_H */
