#ifndef __TYPE__
#define __TYPE__

#include <stdint.h>
typedef int bool;
#define true 1
#define false 0
#define uint32_t unsigned int

#define BYTE unsigned char
typedef BYTE byte;

#define SYS_FUNCTION __attribute__((section(".system_code")))

#define SYS_DATA __attribute__((section(".system_data")))
#define SYS_INIT __attribute__((section(".sys_init")))
#define USER_INIT __attribute__((section(".user_init")))

#define WEAK __attribute__((weak))

#define xlen_t unsigned int
#define xlen 32

#endif