/* hal_shim.c  -  stage 1 TinyRocket bring-up SHIM
 *
 * Mock to run CLINT/rtc on TinyRocket standalone, without S5740 peripherals/host (Vseq).
 *   - inpdw(0x2B00/0x2B04): emulated as a monotonically increasing 64-bit RTC counter
 *   - other addresses: small RAM backing (outpdw stores -> inpdw retrieves)
 *   - exit(): terminate via tohost (riscv-tests convention)
 *
 * Depends: link.ld of the TinyRocket bring-up BSP (App/bringup/common) must define
 *       the 'tohost' symbol (already satisfied if baremetal hello PASSes).
 */
#include "S5740_test.h"

/* riscv-tests convention symbols. Defined by BSP link.ld. */
extern volatile uint64_t tohost;

/* ---------- RTC mock (CLINT/rtc only) ----------
 * Test logic: read time as 64-bit, verify monotonic increase,
 *   success when time >= 0x7EADBEEFDEADBEEF + 0x1000000.
 * Latch snapshot on 0x2B00(lo) read, advance after 0x2B04(hi) read -> prevent lo/hi tearing. */
#define RTC_LO_ADDR  0x2B00u
#define RTC_HI_ADDR  0x2B04u
#define RTC_STEP     0x40000ull   /* reach the 0x1000000 target within ~64 reads */

static uint64_t rtc_counter  = 0x7EADBEEFDEADBEEFull; /* near the test expected start */
static uint64_t rtc_snapshot = 0;

/* other MMIO is backed by small RAM (store/retrieve for TEST_ADDR etc.) */
#define SHADOW_WORDS 256
static uint32_t shadow[SHADOW_WORDS];

/* Mailbox region shared with host(Vseq). In co-sim the host accesses this DRAM
 * region directly via master_write/read, so the DUT must also see DRAM directly (not shadow)
 * so host and DUT see the same memory. (Same principle as the DEMO/handshake firmware
 * accessing via volatile* to PASS. PLIC goes through inpdw/outpdw, so a branch is needed.) */
#define HOST_MBX_LO  0x80010000u
#define HOST_MBX_HI  0x80020000u   /* extended: mailbox(0x80010000) + BUF(0x80011000..) */

/* -- real MMIO passthrough (added for the rt-dev ported test case) --
 * The rt-dev test does outpdw(SW_INT=0x02000000, 0) expecting a REAL CLINT write.
 * Without this branch it would land in shadow[] and no interrupt would ever clear/fire.
 * PLIC is accessed via volatile* macros (interrupt.h) so it does not strictly need this,
 * but the range is passed through for consistency. */
#define CLINT_LO     0x02000000u
#define CLINT_HI     0x02010000u
#define PLIC_LO      0x0C000000u
#define PLIC_HI      0x10000000u
#define IS_REAL_MMIO(a) (((a) >= CLINT_LO && (a) < CLINT_HI) || \
                         ((a) >= PLIC_LO  && (a) < PLIC_HI))

/* PMU counter mock (PLIC/latency only).
 *   PMU_COUNTER_ADDR(id) = 0x2C00 + (id<<4). PMUCounter bitfields:
 *   { triggerCnt:8, completeCnt:8, accLatency:16 } -> accLatency is the upper 16 bits.
 *   Give a fixed mock latency so the latency-measuring firmware gets a non-zero value
 *   and exits while(g_latency==0). (Real values are filled by HW PMU during RTL CA measurement.) */
#define PMU_BASE     0x2C00u
#define PMU_TOP      0x2D00u
#define PMU_MOCK_LATENCY  42u   /* mock: accLatency=42 cycle */

uint32_t inpdw(uint32_t addr)
{
    if (addr == RTC_LO_ADDR) {
        rtc_snapshot = rtc_counter;                 /* latch */
        return (uint32_t)(rtc_snapshot & 0xFFFFFFFFu);
    }
    if (addr == RTC_HI_ADDR) {
        uint32_t hi = (uint32_t)(rtc_snapshot >> 32);
        rtc_counter += RTC_STEP;                     /* advance after full read */
        return hi;
    }
    if (addr >= PMU_BASE && addr < PMU_TOP) {
        /* PMUCounter: accLatency is the upper 16 bits -> (latency<<16). */
        return ((uint32_t)PMU_MOCK_LATENCY << 16);
    }
    if (addr >= HOST_MBX_LO && addr < HOST_MBX_HI) {
        return *(volatile uint32_t *)addr;           /* host mailbox + BUF: DRAM direct */
    }
    if (IS_REAL_MMIO(addr)) {
        return *(volatile uint32_t *)addr;           /* CLINT/PLIC: real MMIO */
    }
    return shadow[(addr >> 2) & (SHADOW_WORDS - 1)];
}

void outpdw(uint32_t addr, uint32_t val)
{
    if (addr >= HOST_MBX_LO && addr < HOST_MBX_HI) {
        *(volatile uint32_t *)addr = val;            /* host mailbox + BUF: DRAM direct */
        return;
    }
    if (IS_REAL_MMIO(addr)) {
        *(volatile uint32_t *)addr = val;            /* CLINT/PLIC: real MMIO */
        return;
    }
    shadow[(addr >> 2) & (SHADOW_WORDS - 1)] = val;
}

/* ---------- exit -> tohost ----------
 * riscv-tests: PASS = tohost 1, FAIL(code) = (code<<1)|1.
 * CODE_SUCCESS(=0) also safely maps to PASS. */
void exit(int code)
{
    tohost = (code == CODE_SUCCESS) ? (uint64_t)1
                                    : (((uint64_t)code << 1) | 1u);
    for (;;) { }
}
