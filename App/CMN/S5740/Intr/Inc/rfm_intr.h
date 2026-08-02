#ifndef _RFM_INTR_H_
#define _RFM_INTR_H_

#include "S5730_api.h"

#define INTR_ID_OFFSET 1
#define PRI_L bottom_priority
#define PRI_M default_priority
#define PRI_H top_priority
#define REGISTER_RFM_EXTINT_HANDLER(id, pri, hnd) api_register_extint_handler(id + INTR_ID_OFFSET, hnd, pri)

typedef enum {
    RFM_BOOT_INTR = 0,
    RFM_MBOX0_INTR = 1,
    RFM_MBOX1_INTR = 2,
    RFM_MBOX2_INTR = 3,
    RFM_MBOX3_INTR = 4,
    RFM_MBOX4_INTR = 5,
    RFM_MBOX5_INTR = 6,
    RFM_MBOX6_INTR = 7,
    RFM_MBOX7_INTR = 8,
    RFM_MBOX8_INTR = 9,
    RFM_MBOX9_INTR = 10,
    RFM_MBOX10_INTR = 11,
    RFM_MBOX11_INTR = 12,

    RFM_RX_OFF_TICK_1 = 13,
    RFM_RX_OFF_TICK_0 = 14,
    RFM_RX_ON_TICK_1 = 15,
    RFM_RX_ON_TICK_0 = 16,
    RFM_TX_OFF_TICK_1 = 17,
    RFM_TX_OFF_TICK_0 = 18,
    RFM_TX_ON_TICK_1 = 19,
    RFM_TX_ON_TICK_0 = 20,
    RFM_LATCH_TICK_TX_1 = 21,
    RFM_LATCH_TICK_TX_0 = 22,
    RFM_LATCH_TICK_RX_1 = 23,
    RFM_LATCH_TICK_RX_0 = 24,

    RFM_IPC_WRITE = 25,
    RFM_IPC_READ = 26,
    RFM_SPEEDY_ERR = 27,
} rfm_intr_t;

extern void intr_RegisterRfmSwInterrupts(void);

#endif
