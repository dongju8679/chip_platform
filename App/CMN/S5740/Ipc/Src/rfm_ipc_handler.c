#include "rfm_ipc_handler.h"
#include "S5730_api.h"

u32 mcu_test_mode = 0;
u32 mcu_test_cnt = 0;

#undef RFM_IPC_DEF
#define RFM_IPC_DEF(ipc, pri) WEAK void ipcHndlr_##ipc(u8 ch, rfm_ipc_msg_t *pmsg) {}
#include "rfm_ipc_def.h"
#undef RFM_IPC_DEF
