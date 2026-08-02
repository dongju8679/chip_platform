#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

typedef struct _ACCESS_ERROR_INFO {
    unsigned int slave : 6;
    unsigned int write : 2;
    unsigned int count :24;
    unsigned int address;
    unsigned int data;
} ACCESS_ERROR_INFO;

typedef struct _EXIT_INFO {
    unsigned int mcause : 4;
    unsigned int mepc : 20;
    unsigned int ecode : 8;
    unsigned int exit_cycles :32;
    union {
        struct _ACCESS_ERROR_INFO access_info;
    };
} EXIT_INFO;

#define pEXIT_INFO (*((EXIT_INFO **)EXIT_LOG_BASE_ADDR))
extern EXIT_INFO gEXIT_INFO;

void exit(unsigned int code);
void alert_master();
void dump_gprs();
void enable_debug_interrupt();

int user_exception_handler();
int exception_handler(xlen_t cause);

#endif
