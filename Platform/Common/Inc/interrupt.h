#ifndef __INTERRUPT__
#define __INTERRUPT__

#if __riscv_xlen == 64
#define _INT_BIT (1<<63)
#elif __riscv_xlen == 32
#define _INT_BIT (1<<31)
#elif __riscv_xlen == 32
#else
#error "No __riscv_xlen defined."
#endif

#include "encoding.h"
#include "type.h"
#include "util.h"

#define _INT_U_SW_INT    (_INT_BIT|0)
#define _INT_S_SW_INT    (_INT_BIT|1)
#define _INT_H_SW_INT    (_INT_BIT|2)
#define _INT_M_SW_INT    (_INT_BIT|3)
#define _INT_U_TIMER_INT (_INT_BIT|4)
#define _INT_S_TIMER_INT (_INT_BIT|5)
#define _INT_H_TIMER_INT (_INT_BIT|6)
#define _INT_M_TIMER_INT (_INT_BIT|7)
#define _INT_U_EXT_INT   (_INT_BIT|8)
#define _INT_S_EXT_INT   (_INT_BIT|9)
#define _INT_H_EXT_INT   (_INT_BIT|10)
#define _INT_M_EXT_INT   (_INT_BIT|11)
#define _INT_RESERVED    (_INT_BIT|12)

/* non-underscore aliases used by syscalls.c trap_handler (rt-dev quirk) */
#define INT_U_SW_INT     (_INT_BIT|0)
#define INT_U_TIMER_INT  (_INT_BIT|4)
#define INT_U_EXT_INT    (_INT_BIT|8)
#define INT_RESERVED     (_INT_BIT|12)

#define CLAIM_PLIC_INT() (*(volatile unsigned long long*)PLIC_CLAIM)
#define COMPLETE_PLIC_INT(id) (*(volatile unsigned long long*)PLIC_CLAIM)=id

#define SET_PLIC_THRESHOLD(val) (*(volatile unsigned int*)PLIC_THRESHOLD)=val
#define READ_PLIC_THRESHOLD() (*(volatile unsigned int *)PLIC_THRESHOLD)

#define ENABLE_ALL_PLIC_INT() \
outpdw(PLIC_ENABLE, 0x1fffffffffff);
#define ENABLE_PLIC_INT(id) \
(*(volatile unsigned long long*)PLIC_ENABLE) = \
(*(volatile unsigned long long*)PLIC_ENABLE)|(1ull<<id)

#define DISABLE_ALL_PLIC_INT() \
(*(volatile unsigned long long*)PLIC_ENABLE) = 0ull
#define DISABLE_PLIC_INT(id) \
(*(volatile unsigned long long*)PLIC_ENABLE) = \
((*(volatile unsigned long long*)(PLIC_ENABLE))&~(1llu<<id))

#define SET_PLIC_INT_PRIORITY(id, pri) \
(*(volatile unsigned int*)(PLIC_PRIORITY+(id*4))) = pri
#define READ_PLIC_INT_PRIORITY(id) \
(*(volatile unsigned int*)(PLIC_PRIORITY+(id*4)));

#define SET_INTERRUPT_ENABLE(ie) set_csr(mie, ie)
#define CLEAR_INTERRUPT_ENABLE(ie) clear_csr(mie, ie)

#define GLOBAL_INTERRUPT_ENABLE() set_csr(mstatus, MSTATUS_MIE)
#define GLOBAL_INTERRUPT_DISABLE() clear_csr(mstatus, MSTATUS_MIE)

#define START_OF_ISR 0xF0000000
#define RET_ERROR 0x1FFFFFFF
#define RET_HANDLER_DONE 0
#define RET_NO_HANDLER 0x1FFFFFF8
#define RET_NO_INT 0x1FFFFFF0
#define RET_FAULT 0xFFFFFFFF

#define _ALL_IE_BITS 0xfff
#define _ALL_IP_BITS 0xfff

enum _enum_ie_ {
USIE = 0x001, SSIE=0x002, HSIE=0x004, MSIE=0x008,
UTIE = 0x010, STIE=0x020, HTIE=0x040, MTIE=0x080,
UEIE = 0x100, SEIE=0x200, HEIE=0x400, MEIE=0x800,
};

enum _enum_ip_ {
USIP = 0x001, SSIP=0x002, HSIP=0x004, MSIP=0x008,
UTIP = 0x010, STIP=0x020, HTIP=0x040, MTIP=0x080,
UEIP = 0x100, SEIP=0x200, HEIP=0x400, MEIP=0x800,
};

#ifndef PRIORITY_ENUM
#define PRIORITY_ENUM
enum _priority {
bottom_priority = 2,
default_priority = 3,
ipc_priority = 4,
top_priority = 5,
apb_error_priority = 6,
debug_priority = 7
};
#endif

typedef struct _ISR_GROUP{
int (*ext_int_handler[PLIC_NUMBER_OF_INTERRUPTS+1])(void);
int (*sw_int_handler)(void);
int (*time_int_handler)(void);
} ISR_GROUP;

extern ISR_GROUP pISR_BASE;

int empty(void);
void init_extint_handler(void);
int _register_extint_handler(int num, int (*handler)());
int register_extint_handler(int num, int (*handler)(), enum _priority pri);
#endif