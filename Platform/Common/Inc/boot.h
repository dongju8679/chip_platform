#ifndef __BOOT__
#define __BOOT__

/* boot.h - the boot sequence (reset -> main) and the C-side init entry point
 *
 * -- Actual boot order (Platform/Chipset/TinyRocket/crt.S) ------------
 *   0x80000000  _start
 *     1. _init_gpr    : x2..x31 = 0
 *     2. _init_csr    : mtval = 0, mtvec = 0
 *     3. gp           : la gp, __global_pointer$    (link.ld)
 *     4. sp           : la sp, __stack_top          (link.ld, end of ram)
 *     5. .bss clear   : zero __bss_start .. __bss_end 4 bytes at a time
 *     6. mtvec        : install _trap_vector_entry (mtvec.S), MODE=Direct
 *     7. bp_init()    : mailbox handshake with the host (Vseq)
 *                       skipped under -DPRELOAD (util.c)
 *     8. set_log(1)   : record the starting mcycle value (util.c)
 *     9. init_fpu()   : only with -DFPU. rv32imac has no F, so skipped by default
 *    10. main()
 *    11. exit()       : rtdev_shim.c -> tohost
 *
 * -- What this header provides ---------------------------------------
 * The order above could previously only be learned "from documentation, not
 * from code" - you had to read crt.S to find out where sp lives and when mtvec
 * is installed.
 * Two things are provided here:
 *   (1) accessors that let C inspect the boot result (boot_info_t)
 *       -> a test can verify "did it really happen in that order".
 *   (2) boot_init_hal(), collecting the C-side initialization needed after
 *       crt.S in one place
 *       -> new tests no longer have to spell out exceptions_init() +
 *          uart_init() ... by hand every time.
 *
 * -- Why crt.S was not rewritten in C --------------------------------
 * crt.S runs with neither a stack nor gp established. Moving it to C would
 * still require "assembly to call C". And touching crt.S changes the .text.init
 * size, which changes the images of all existing features (cost: re-running the
 * regression). So crt.S is left as is and this file only owns the stages after
 * it.
 *
 * -- Is this verifiable on chipyard? ---------------------------------
 * Yes. sp/gp/mtvec/.bss are all readable from CSRs and memory.
 * verif/ip/S5740/EXCEPTION/traps calls boot_check() at startup.
 */

#include "type.h"

/* Snapshot of the state right after boot */
typedef struct _boot_info {
    xlen_t sp;          /* current stack pointer */
    xlen_t gp;          /* __global_pointer$ */
    xlen_t mtvec;       /* installed trap vector (low 2 bits = MODE) */
    xlen_t stack_top;   /* link.ld __stack_top */
    xlen_t bss_start;
    xlen_t bss_end;
    xlen_t heap_start;  /* link.ld __heap_start */
} boot_info_t;

/* Read the current state and fill it in. */
void boot_get_info(boot_info_t *info);

/* Self-check that boot completed correctly.
 *   - is sp inside [__heap_start, __stack_top]
 *   - is gp non-zero
 *   - is mtvec non-zero with MODE=Direct (low 2 bits clear)
 *   - is .bss entirely zero (data_init_bss_is_zero)
 * Returns: 0 = OK. Non-zero is a bitmask of the failing checks (BOOT_ERR_x).
 * IMPORTANT: because of the .bss check, call it immediately after entering
 * main().
 */
#define BOOT_ERR_SP     (1u << 0)
#define BOOT_ERR_GP     (1u << 1)
#define BOOT_ERR_MTVEC  (1u << 2)
#define BOOT_ERR_BSS    (1u << 3)
unsigned boot_check(void);

/* Perform all C-side initialization that follows crt.S at once.
 *   - uart_init()      : the stdout path (printf/LOG_x)
 *   - exceptions_init(): replace mtvec with the shared exception entry point
 * IMPORTANT: exceptions_init() takes over mtvec, so a test that wants to keep
 *   using the rt-dev interrupt path (_trap_vector_entry -> trap_handler) must
 *   not call this function. Interrupts are still forwarded to pISR_BASE by
 *   trap_dispatch() exactly the same way, but be aware that the mtvec address
 *   changes.
 */
void boot_init_hal(void);

#endif /* __BOOT__ */
