#ifndef __DATA_INIT__
#define __DATA_INIT__

/* data_init.h - .data initialization / .bss clearing (C runtime preparation)
 *
 * -- Who does it today -----------------------------------------------
 * crt.S (Platform/Chipset/TinyRocket/crt.S) does it:
 *     la t0, __bss_start ; la t1, __bss_end ; fill with 0, 4 bytes at a time
 * Nobody copies .data. And that is correct - see below.
 *
 * -- Why there is no .data copy (a fact about this platform) ---------
 * link.ld does not separate LMA (load address) from VMA (run address).
 * It simply lays .text/.rodata/.data back to back in a single ram region
 * (0x80000000, 60K). The image is then loaded by fesvr/TSI writing the ELF
 * straight into DRAM (there is no ROM-to-RAM copy stage).
 * So .data already sits at its execution address with the correct contents.
 * There is nothing to copy.
 *
 * This API therefore exists not because it is needed today, but for two
 * reasons:
 *   (1) Verification: expose the linker symbols to C so a test can check
 *       whether .bss really is zero and where its boundaries are. Until now
 *       they lived only inside crt.S.
 *   (2) Portability: a real ROM boot (XIP) or a move to TCM will need the
 *       .data copy. The slot is reserved here so only data_init_data() has to
 *       be filled in at that point. It is a no-op today because the linker does
 *       not provide __data_load_start.
 *
 * -- Why crt.S was not replaced with this ----------------------------
 * Replacing the .bss loop in crt.S with a C call would change the .text.init
 * size and shift every symbol address after it -> the images of all existing
 * features would change, forcing the full regression to be re-verified from
 * scratch. There is nothing to gain.
 * What crt.S does today and what data_init_bss() does here are identical.
 * (To self-check that they are, use data_init_bss_is_zero() below.)
 *
 * -- Is this verifiable on chipyard? ---------------------------------
 * Yes. Read the .bss boundary symbols and confirm the whole range is zero.
 * verif/ip/S5740/EXCEPTION/traps performs this check at startup.
 */

#include "type.h"

/* The .bss boundaries supplied by the linker (__bss_start / __bss_end in
 * link.ld). The boundary is "the address of the symbol", not its value. */
void data_init_bss_range(xlen_t *start, xlen_t *end);

/* Zero-fill .bss. crt.S already did this, so it is normally not needed.
 * It is idempotent and safe to call again - but it also wipes globals that
 * already hold values, so do not call it after entering main. */
void data_init_bss(void);

/* Copy .data from LMA to VMA.
 * With the current link.ld, LMA==VMA, so it does nothing (see above).
 * Only this function needs filling in when moving to a ROM boot. */
void data_init_data(void);

/* data_init_data() + data_init_bss() */
void data_init(void);

/* Check whether the whole .bss range is zero. 1 = all zero.
 * IMPORTANT: only meaningful right after boot, before any global is written.
 *   It is a self-check that the clear in crt.S actually worked. */
int  data_init_bss_is_zero(void);

#endif /* __DATA_INIT__ */
