#ifndef S5740_RT3_MAP
#define S5740_RT3_MAP

/* -- chipyard port --
 * Converts the addresses of the original rt-dev S5740.h to the chipyard
 * standard addresses.
 * The sw_regmap.h / hw_regmap.h / hw_memmap.h headers the original included do
 * not exist on chipyard and were removed (add them back if a symbol from them
 * is ever needed).
 *
 * CLINT base = 0x02000000, PLIC base = 0x0C000000 (chipyard standard).
 */

#define PLIC_NUMBER_OF_INTERRUPTS 48

/* -- PLIC (base 0x0C000000) --
 * priority: base + id*4
 * pending : base + 0x1000
 * enable  : base + 0x2000 + hart*0x80   (hart0 M-mode = +0x2000)
 * threshold: base + 0x200000 + hart*0x1000 (hart0 M-mode = +0x200000)
 * claim/complete: threshold + 4
 */
#define PLIC_PRIORITY   0x0C000000
#define PLIC_PENDING    0x0C001000
#define PLIC_ENABLE     0x0C002000
#define PLIC_THRESHOLD  0x0C200000
#define PLIC_CLAIM      0x0C200004
#define PLIC_CLEAR      0x0C200004
#define BOOT_INTERRUPT  0x1
#define PLIC_MAX_PRIORITY 0x7

/* -- CLINT (base 0x02000000) --
 * MSIP     : base + 0x0000  (SW interrupt)
 * MTIMECMP : base + 0x4000
 * MTIME    : base + 0xBFF8
 */
#define CLINT_SW_INTERRUPT 0x02000000
#define CLINT_TIMECMP_LO   0x02004000
#define CLINT_TIMECMP_HI   0x02004004
#define CLINT_TIME_LO      0x0200BFF8
#define CLINT_TIME_HI      0x0200BFFC

#define SEGMENT_CONFIG_REGISTER 0x805

#endif
