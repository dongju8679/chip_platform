#include "rfm_ipc_msg.h"
#include "rfm_ipc_handler.h"
#include "rfm_hal_dif.h"

#define IPC_BUFFER_CLEAR
#if defined(IPC_BUFFER_CLEAR)
#define IPC_READ(dst, src, len) {api_memory((void*)(dst), (const void *)(src), (len)); api_memset((void *)(src), 0, (len)); }
#else
#define IPC_READ(dst, src, len) api_memory((void*)(dst), (const void *)(src), (len))
#endif

#undef RFM_IPC_DEF
#define RFM_IPC_DEF(ipc, pri) ipcHndlr_##ipc,
static const ipc_handler_t ipc_handler[NUM_OF_RFM_IPC] = {
    #include "rfm_ipc_def.h"
};
#undef RFM_IPC_DEF
#define RFM_IPC_DEF(ipc, pri) sizeof(rfm_ipc_hdr_t) + sizeof(IPC_##ipc##_t),
static const u8 ipc_msg_size[NUM_OF_RFM_IPC] = {
    #include "rfm_ipc_def.h"
};
#undef RFM_IPC_DEF
static const u32 ipc_buf_addr[NUM_OF_RFM_IPC_CH] = {RFM_IPC_BUFFER_APB_ADDR0, RFM_IPC_BUFFER_APB_ADDR1};
static rfm_ipc_msg_t ipc_msg = {0};

u32 ipc_skip_cnt[NUM_OF_RFM_IPC_CH] = {0};

void ipc_ReceiveRfmMsg(u8 ch, u8 idx, u32 arg)
{
    *(u32 *)&ipc_msg = arg;
    u8 type = ipc_msg.hdr.type;
    u8 length = ipc_msg.hdr.length;

    if (type == RFM_BB_INIT && length == ipc_msg_size[type]) {
        ipc_skip_cnt[ch] = 0;
    }

    if (type < NUM_OF_RFM_IPC && length > 0 && length <= ipc_msg_size[type]) {
        if (length > sizeof(u32)) {
            u32 buf_addr = ipc_buf_addr[ch] + idx + MAX_RFM_IPC_MSG_SIZE;
            IPC_READ((u32 *)&ipc_msg + 1, buf_addr, length - sizeof(u32));
        }

        if (ipc_handler[type] != 0)
            ipc_handler[type](ch, &ipc_msg);

    }
    else {
        ipc_skip_cnt[ch]++;
        hal_SetRfmUserData(RFM_USER_ADDR_IPC_SKIP0 + ch, ipc_skip_cnt[ch]);
    }
    hal_SetRfmUserData(RFM_USER_ADDR_IPC_DONE0 + ch, *(u32 *)&ipc_msg);
}
