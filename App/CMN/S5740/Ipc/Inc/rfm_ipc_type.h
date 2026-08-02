#ifndef _RFM_IPC_TYPE_H_
#define _RFM_IPC_TYPE_H_

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef int BOOL32;
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef short s16;
typedef unsigned int u32;
typedef int s32;

#ifdef __GNUC__
typedef unsigned long long int u64;
typedef long long int s64;
#else
typedef unsigned __int64 u64;
typedef __int64 s64;
#endif

#include "rfm_L1_ipc_type.h"

#include "RFM_RF_IPC_Type.h"

#endif
