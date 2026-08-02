#ifndef __I2SR_H__
#define __I2SR_H__

#ifdef I2SR
#define I2SR_ENABLE {asm volatile("csrwi 0x7c2, 1"); asm volatile("csrwi 0x7c4, 2");}
#define REGISTER_EXTINT_HNDLR(id, hndlr, pri) i2sr_register_ext_int_handler(id, (int (*)(int))hndlr, pri)
#define IPC_HNDLR ((int (*)(int))((unsigned int)ipc|1))
#define TRAP_ENTRY __attribute__((fast_interrupt("machine")))
#else
#define REGISTER_EXTINT_HNDLR register_ext_int_handler
#define IPC_HNDLR ((int (*)(int))ipc)
#define TRAP_ENTRY __attribute__((interrupt("machine")))
#endif

void register_handler(unsigned int, void *);
int i2sr_register_ext_int_handler(int, int (*)(int), int);

#define CSR_MINTSTATUS 0x346
#define CSR_MSILEVEL 0x7c3
#define CSR_MTILEVEL 0x7c4
#define CSR_CURRID 0x7c5
#define csr_write(csr, val) write_csr(csr, val)
#define csr_read(csr) read_csr(csr)

void ISR();

#endif
