#include "rfm_intr.h"
#include "rfm_ipc_msg.h"

static int intHndlr_RFM_SW_INTR_IPC_0(int arg);
static int intHndlr_RFM_SW_INTR_IPC_1(int arg);

void intr_RegisterRfmSwInterrupts(void)
{
    REGISTER_RFM_EXTINT_HANDLER(RFM_MBOX0_INTR, PRI_M, intHndlr_RFM_SW_INTR_IPC_0);
    REGISTER_RFM_EXTINT_HANDLER(RFM_MBOX1_INTR, PRI_M, intHndlr_RFM_SW_INTR_IPC_0);
    REGISTER_RFM_EXTINT_HANDLER(RFM_MBOX2_INTR, PRI_M, intHndlr_RFM_SW_INTR_IPC_0);
    REGISTER_RFM_EXTINT_HANDLER(RFM_MBOX3_INTR, PRI_M, intHndlr_RFM_SW_INTR_IPC_0);
    REGISTER_RFM_EXTINT_HANDLER(RFM_MBOX4_INTR, PRI_M, intHndlr_RFM_SW_INTR_IPC_1);
    REGISTER_RFM_EXTINT_HANDLER(RFM_MBOX5_INTR, PRI_M, intHndlr_RFM_SW_INTR_IPC_1);
    REGISTER_RFM_EXTINT_HANDLER(RFM_MBOX6_INTR, PRI_M, intHndlr_RFM_SW_INTR_IPC_1);
    REGISTER_RFM_EXTINT_HANDLER(RFM_MBOX7_INTR, PRI_M, intHndlr_RFM_SW_INTR_IPC_1);
}

int intHndlr_RFM_SW_INTR_IPC_0(int arg)
{
    int mbx = arg - (RFM_MBOX0_INTR + INTR_ID_OFFSET);
    ipc_ReceiveRfmMsg(0, mbx - 0, inpdw(SYS_MBOX0_ADDR + mbx * 4));
    return 0;
}

int intHndlr_RFM_SW_INTR_IPC_1(int arg)
{
    int mbx = arg - (RFM_MBOX0_INTR + INTR_ID_OFFSET);
    ipc_ReceiveRfmMsg(1, mbx - 4, inpdw(SYS_MBOX0_ADDR + mbx * 4));
    return 0;
}
