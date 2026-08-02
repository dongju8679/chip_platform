#ifndef _RFM_IPC_HANDLER_H_
#define _RFM_IPC_HANDLER_H_

#include "rfm_ipc_msg.h"

extern u32 mcu_test_mode;
extern u32 mcu_test_cnt;

#undef RFM_IPC_DEF
#define RFM_IPC_DEF(ipc, pri) extern void ipcHndlr_##ipc(u8 ch, rfm_ipc_msg_t *pmsg );
#include "rfm_ipc_def.h"
#undef RFM_IPC_DEF

#endif
