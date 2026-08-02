#include "type.h"
#include "encoding.h"
#include "util.h"
#include "interrupt.h"
#include "syscalls.h"
#include "uart.h"

int error_counter=0;
int mcause_value[4];

/* -- newlib-compatible _write(): stdout/stderr -> chipyard UART0 -------
 * printf/puts/putchar (Platform/Common/Src/printf.c) ultimately land here.
 * Only fd 1 (stdout) / 2 (stderr) go to the UART; everything else returns -1.
 * Completely independent of the existing IP verification (mailbox handshake)
 * path.
 */
int _write(int fd, const char *buf, int len)
{
    if (fd != 1 && fd != 2) return -1;
    if (!buf || len <= 0)   return 0;
    uart_write(buf, (unsigned int)len);
    return len;
}

xlen_t trap_handler(xlen_t cause)
{
    /* exception handler*/
    if ((cause & _INT_BIT)==0)
    {
        exit(CODE_FATAL);
        return 1;
    }
    xlen_t ret = 0;
    if(cause == _INT_M_EXT_INT)
    {
        xlen_t plic_claim = inpdw(PLIC_CLAIM);
        outpdw(PLIC_CLEAR, plic_claim);
        ret = pISR_BASE.ext_int_handler[plic_claim]();
    }
    else if (cause == _INT_M_TIMER_INT)
    {
        ret = pISR_BASE.time_int_handler();
    }
    else{
        if (cause == _INT_M_SW_INT)
            ret = pISR_BASE.sw_int_handler();
        else if (cause == INT_U_EXT_INT);
        else if (cause == INT_U_SW_INT);
        else if (cause == INT_U_TIMER_INT);
        else if (cause == INT_RESERVED);
        else{
            exit(CODE_FATAL);
            return 0;
        }        
    }
    if(ret != 0){
        exit(CODE_FAIL);
    }
    return 0;
}

#ifdef FPU
int init_fpu() {
    set_csr(mstatus, MSTATUS_FS);
    clear_csr(fcsr, 0xE0);
    set_csr(fcsr, 0x80);
    return CODE_SUCCESS;
}
#else
/* rv32imac has no F extension; fcsr access would be illegal. */
int init_fpu() { return CODE_SUCCESS; }
#endif

xlen_t trap_handler_PMP(xlen_t cause)
{
    if((cause & _INT_BIT)==0)
    {
        mcause_value[0]=cause;
        if((cause==0x5)||(cause==0x7)){
            error_counter+=1;
            asm volatile("lw ra,12(sp)");
            asm volatile("add ra,ra,2");
            asm volatile("sw ra,12(sp)");
            return 1;            
        }
        else if(cause==0x1){
            error_counter+=1;
            asm volatile("lw ra,12(sp)");
            asm volatile("add ra,ra,4");
            asm volatile("sw ra,12(sp)");
            return 1;
        }
        else{
            exit(CODE_FATAL);
            return 1;
        }
    }
    xlen_t ret = 0;
    if (cause == _INT_M_EXT_INT)
    {
        xlen_t plic_claim = inpdw(PLIC_CLAIM);
        outpdw(PLIC_CLEAR, plic_claim);
        ret = pISR_BASE.ext_int_handler[plic_claim]();
        return 0;
    }
    else if(cause==_INT_M_TIMER_INT)
    {
        ret = pISR_BASE.time_int_handler();
    }
    else{
        if(cause == _INT_M_SW_INT)
            ret = pISR_BASE.sw_int_handler();
        else if (cause == INT_U_EXT_INT);
        else if (cause == INT_U_SW_INT);
        else if (cause == INT_U_TIMER_INT);
        else if (cause == INT_RESERVED);
        else{
            exit(CODE_FATAL);
            return 0;
        }
    }
    if(ret != 0){
        exit(CODE_FAIL);
    }
    return 0;
}
