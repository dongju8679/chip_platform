/* I2SR_int_muldiv.c - rt-dev original test case, ported to chipyard RV32Rocket.
 *
 * [Port policy] Test body is rt-dev's, kept as-is (structure/flow/ISRs unchanged).
 *   Only the following were necessary for it to compile/run on chipyard:
 *     1. #include "interrupt.h"   (rt-dev pulled it via S5740_test.h)
 *     2. duplicate `volatile unsigned int rdata;` removed (isr2..isr5) - C requires
 *        a single definition; rt-dev built it as one TU with a lax compiler.
 *     3. addresses come from I2SR_int_muldiv.h (chipyard values)
 *   Everything else (vectored trap, 8 ISRs, nested-interrupt GLOBAL_INTERRUPT_ENABLE,
 *   muldiv workload, BP handshake, I2SR register_handler) is rt-dev verbatim.
 */
#include "S5740_test.h"
#include "I2SR_int_muldiv.h"
#include "interrupt.h"

/* -- debug progress marker --
 * Writes a step number to 0x80010020 so the host can report where the DUT stopped
 * when a BP handshake times out. Always on while bringing this test up.
 * (Remove the DBG() calls once the test is stable, to get the rt-dev path byte-for-byte.) */
#define DBG(n) outpdw(0x80010020u, (n))

#define CSR_MINTSTATUS 0x346
#define CSR_MSILEVEL 0x7c3
#define CSR_MTILEVEL 0x7c4
#define CSR_CURRID 0x7c5
#define csr_write(csr, val) write_csr(csr, val)
#define csr_read(csr) read_csr(csr)

void register_handler(unsigned int id, void* handler){
volatile unsigned int inst[2];
inst[0] = ((0xBC0 + id -1) << 20)| (10 << 15) | MATCH_CSRRW;
inst[1] = 0x8082; // RET
asm volatile("fence.i");
((void (*)(void *))inst)(handler);
}

void exception(){
exit(1);
}

int inner_count;

#ifdef I2SR
/* rt-dev original. With I2SR the hardware fast-vectors straight to isr1..5, so external()
 * is never entered through the trap table - that is why it carries no interrupt attribute
 * and reads the id from CSR_CURRID(0x7c5). */
void external() {
unsigned int id = csr_read(CSR_CURRID);
#else
/* Without I2SR this IS the external path: PLIC -> mcause 11 -> trap table's `j external`.
 * Entered that way it must save/restore and mret, and the id comes from a PLIC claim
 * (which must be completed at the end). */
__attribute__((interrupt("machine")))
void external() {
unsigned int id = CLAIM_PLIC_INT();
#endif
unsigned int time = read_csr(mcycle);
inner_count++;

GLOBAL_INTERRUPT_ENABLE();
struct BUF * buf = &(((struct BUF*)BUF_ADDR)[id]);
unsigned int (*data)[2] = buf->data;
for(int i = 0; i < BUF_SIZE; i++) {
data[i][1] += data[i][0];
data[i][1] /= 7;
data[i][1] *= 3;
}

time = read_csr(mcycle) - time;
if(time < buf->min_time) buf->min_time = time;
if(time > buf->max_time) buf->max_time = time;
buf->tot_time += time;
buf->count++;
GLOBAL_INTERRUPT_DISABLE();
#ifndef I2SR
COMPLETE_PLIC_INT(id);
#endif

}

__attribute__((interrupt("machine")))
void software() {
unsigned int time = read_csr(mcycle);
outpdw(SW_INT, 0);
inner_count++;
GLOBAL_INTERRUPT_ENABLE();
struct BUF * buf = &(((struct BUF*)BUF_ADDR)[6]);
unsigned int (*data)[2] = buf->data;
for(int i = 0; i < BUF_SIZE; i++) {
data[i][1] += data[i][0];
data[i][1] /= 7;
data[i][1] *= 3;
}

time = read_csr(mcycle) - time;
if(time < buf->min_time) buf->min_time = time;
if(time > buf->max_time) buf->max_time = time;
buf->tot_time += time;
buf->count++;
GLOBAL_INTERRUPT_DISABLE();

}

__attribute__((interrupt("machine")))
void timer() {
unsigned int time = read_csr(mcycle);
*(volatile unsigned long long int *)MTIMECMP = -1;
inner_count++;

GLOBAL_INTERRUPT_ENABLE();
struct BUF * buf = &(((struct BUF*)BUF_ADDR)[7]);
unsigned int (*data)[2] = buf->data;
for(int i = 0; i < BUF_SIZE; i++) {
data[i][1] += data[i][0];
data[i][1] /= 7;
data[i][1] *= 3;
}

time = read_csr(mcycle) - time;
if(time < buf->min_time) buf->min_time = time;
if(time > buf->max_time) buf->max_time = time;
buf->tot_time += time;
buf->count++;
GLOBAL_INTERRUPT_DISABLE();

}

__attribute__((naked, aligned(256)))
void trap() {
asm volatile(".option push");
asm volatile(".option norvc");
asm volatile("j exception");
asm volatile("nop");
asm volatile("nop");
asm volatile("j software");
asm volatile("nop");
asm volatile("nop");
asm volatile("nop");
asm volatile("j timer");
asm volatile("nop");
asm volatile("nop");
asm volatile("nop");
asm volatile("j external");
asm volatile(".option pop");

}


#ifdef I2SR
/* isr1..isr5 - registered into the I2SR fast vector (CSR 0xBC0+) by register_handler().
 * Unreachable without I2SR. rt-dev deliberately mixes the two entry types: isr1 takes the
 * normal interrupt prologue, isr2..5 the I2SR fast one, so one run exercises both.
 * NOTE fast_interrupt is not in upstream RISC-V GCC - building these needs the I2SR-aware
 * toolchain, otherwise the attribute is silently dropped (ret instead of mret). */
volatile unsigned int rdata;
__attribute__((interrupt("machine")))
void isr1(){
unsigned int id = csr_read(CSR_CURRID);
unsigned int time = read_csr(mcycle);
inner_count++;

GLOBAL_INTERRUPT_ENABLE();
struct BUF * buf = &(((struct BUF*)BUF_ADDR)[1]);
unsigned int (*data)[2] = buf->data;
for(int i = 0; i < BUF_SIZE; i++) {
data[i][1] += data[i][0];
data[i][1] /= 7;
data[i][1] *= 3;
}

time = read_csr(mcycle) - time;
if(time < buf->min_time) buf->min_time = time;
if(time > buf->max_time) buf->max_time = time;
buf->tot_time += time;
buf->count++;
GLOBAL_INTERRUPT_DISABLE();
}

/* (rt-dev repeats this line per ISR; C needs one definition) */
__attribute__((fast_interrupt("machine")))
void isr2(){
unsigned int id = csr_read(CSR_CURRID);
unsigned int time = read_csr(mcycle);
inner_count++;

GLOBAL_INTERRUPT_ENABLE();
struct BUF * buf = &(((struct BUF*)BUF_ADDR)[2]);
unsigned int (*data)[2] = buf->data;
for(int i = 0; i < BUF_SIZE; i++) {
data[i][1] += data[i][0];
data[i][1] /= 7;
data[i][1] *= 3;
}

time = read_csr(mcycle) - time;
if(time < buf->min_time) buf->min_time = time;
if(time > buf->max_time) buf->max_time = time;
buf->tot_time += time;
buf->count++;
GLOBAL_INTERRUPT_DISABLE();
}


/* (rt-dev repeats this line per ISR; C needs one definition) */
__attribute__((fast_interrupt("machine")))
void isr3(){
unsigned int id = csr_read(CSR_CURRID);
unsigned int time = read_csr(mcycle);
inner_count++;

GLOBAL_INTERRUPT_ENABLE();
struct BUF * buf = &(((struct BUF*)BUF_ADDR)[3]);
unsigned int (*data)[2] = buf->data;
for(int i = 0; i < BUF_SIZE; i++) {
data[i][1] += data[i][0];
data[i][1] /= 7;
data[i][1] *= 3;
}

time = read_csr(mcycle) - time;
if(time < buf->min_time) buf->min_time = time;
if(time > buf->max_time) buf->max_time = time;
buf->tot_time += time;
buf->count++;
GLOBAL_INTERRUPT_DISABLE();
}

/* (rt-dev repeats this line per ISR; C needs one definition) */
__attribute__((fast_interrupt("machine")))
void isr4(){
unsigned int id = csr_read(CSR_CURRID);
unsigned int time = read_csr(mcycle);
inner_count++;

GLOBAL_INTERRUPT_ENABLE();
struct BUF * buf = &(((struct BUF*)BUF_ADDR)[4]);
unsigned int (*data)[2] = buf->data;
for(int i = 0; i < BUF_SIZE; i++) {
data[i][1] += data[i][0];
data[i][1] /= 7;
data[i][1] *= 3;
}

time = read_csr(mcycle) - time;
if(time < buf->min_time) buf->min_time = time;
if(time > buf->max_time) buf->max_time = time;
buf->tot_time += time;
buf->count++;
GLOBAL_INTERRUPT_DISABLE();
}

/* (rt-dev repeats this line per ISR; C needs one definition) */
__attribute__((fast_interrupt("machine")))
void isr5(){
unsigned int id = csr_read(CSR_CURRID);
unsigned int time = read_csr(mcycle);
inner_count++;

GLOBAL_INTERRUPT_ENABLE();
struct BUF * buf = &(((struct BUF*)BUF_ADDR)[5]);
unsigned int (*data)[2] = buf->data;
for(int i = 0; i < BUF_SIZE; i++) {
data[i][1] += data[i][0];
data[i][1] /= 7;
data[i][1] *= 3;
}

time = read_csr(mcycle) - time;
if(time < buf->min_time) buf->min_time = time;
if(time > buf->max_time) buf->max_time = time;
buf->tot_time += time;
buf->count++;
GLOBAL_INTERRUPT_DISABLE();
}

#endif /* I2SR */

void I2SR_int_muldiv() {
DBG(3);
outpdw(SW_INT, 0);
*(volatile unsigned long long *)MTIMECMP = - 1;
DBG(4);
SET_PLIC_THRESHOLD(0);

DBG(5);
for(int i = 1; i <=5;i++) {
register_extint_handler(i, (void*)0, i);
}
DBG(6);
#ifdef I2SR
register_handler(1, (unsigned int)isr1 | 1);
register_handler(2, (unsigned int)isr2 | 1);
register_handler(3, (unsigned int)isr3 | 1);
register_handler(4, (unsigned int)isr4 | 1);
register_handler(5, (unsigned int)isr5 | 1);
#endif

DBG(7);
write_csr(mtvec, (unsigned int)trap | 1);
write_csr(mie, MSIE | MTIE | MEIE);
outpdw(BREAK_ADDR, 0);
DBG(8);
dut_wait_bp(1);
dut_wait_bp(1);
DBG(9);
dut_set_bp(2);

GLOBAL_INTERRUPT_ENABLE();

DBG(10);
while(inpdw(BREAK_ADDR) == 0) {
DBG(11);
dut_wait_bp(3);
while(inner_count != 5){
}
DBG(12);
inner_count = 0;
dut_set_bp(4);
}
DBG(13);

GLOBAL_INTERRUPT_DISABLE();
}

int main()
{
DBG(1);
#ifdef I2SR
I2SR_ENABLE;                 /* csrwi 0x7c2,1 - needs Support_I2SR */
#endif
DBG(2);
I2SR_int_muldiv();
return CODE_SUCCESS;

}
