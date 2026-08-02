#include "util.h"
#include "memmap.h"
#include "interrupt.h"

LOG log_test;

SYS_FUNCTION void* __wrap_memset(void* _Dst, int _Val, unsigned int _Size)
{
if ((uint32_t)_Dst < ITIM_BASE || (uint32_t)_Dst >= (DTIM_BASE + DTIM_SIZE)) return (void*)CODE_FAIL;
if ((uint32_t)_Dst >= (ITIM_BASE + ITIM_SIZE) && (uint32_t)_Dst < DTIM_BASE) return (void*)CODE_FAIL;

BYTE *buf = (BYTE *)_Dst;
while(_Size--)
{
*buf++ = (BYTE)_Val;
}
return _Dst;
}

SYS_FUNCTION void* __wrap_memcpy(void* _Dst, const void* _Src, unsigned int _Size)
{
if ((uint32_t)_Dst < ITIM_BASE || (uint32_t)_Dst >= (DTIM_BASE + DTIM_SIZE)) return (void*)CODE_FAIL;
if ((uint32_t)_Dst >= (ITIM_BASE + ITIM_SIZE) && (uint32_t)_Dst < DTIM_BASE) return (void*)CODE_FAIL;
BYTE *buf = (BYTE *)_Dst;
BYTE *__Src = (BYTE *)_Src;
while(_Size--)
{
*buf++ = *__Src++;
}
return _Dst;
}

SYS_FUNCTION void* wordcpy(void* _Dst, const void* _Src, unsigned int _Size)
{
if ((int)_Dst & 3) return (void*)CODE_FAIL;
if ((int)_Src & 3) return (void*)CODE_FAIL;
if (_Size & 3) return (void*)CODE_FAIL;

unsigned int* dst = (unsigned int*)_Dst;
unsigned int* src = (unsigned int*)_Src;

while(_Size > 0) {
*dst++ = *src++;
_Size -= 4;

}
return _Dst;


}

unsigned int set_log(unsigned int i)
{
static int first = 1;
if(i){
#if defined(USE_NEWLIB_STDIO) && USE_NEWLIB_STDIO && !defined(NEWLIB_NO_STDIO_INIT)
/* crt.S calls set_log(1) just before main (see the Inc/boot.h comment).
 * When using newlib stdio this is the only C hook guaranteed to run "before the
 * first output", so stdout is made unbuffered here. Without it the first printf
 * mallocs a 1 KB FILE buffer (measured: 1,032 B). See the header comment of
 * Src/newlib_glue.c for details.
 * With USE_NEWLIB_STDIO=0 this block disappears entirely and the image is
 * identical to before. */
newlib_stdio_init();
#endif
log_test.start_time =(read_csr(mcycle)>>8);
}
else if (first){
log_test.end_time = (read_csr(mcycle) >> 8);
log_test.run_time = log_test.end_time - log_test.start_time;
log_test.run_time = (log_test.run_time & 0xFFF);
log_test.run_time = (log_test.run_time << 16);
first = 0;
}
return log_test.run_time;

}


void bp_init()
{
BREAK_POINT = BP_TEST_init;
#ifndef PRELOAD
if(dut_wait_bp(BP_MEM_WRITE)) exit(CODE_FAIL);
#endif
}

void dut_set_bp(unsigned int bp_num)
{
if(BREAK_POINT == BP_END) return;
if(BREAK_POINT == BP_TEST_fail) return;
BREAK_POINT = bp_num;

}

unsigned int dut_wait_bp(unsigned int bp_num)
{
while(1){
if ((bp_num == BP_MEM_WRITE) || (bp_num == BP_TEST_success)) {
if (BREAK_POINT == BP_TEST_begin) return 0;
}

if (BREAK_POINT == bp_num) return 0;
else if (BREAK_POINT == BP_END) {
return 1;
}
else if (BREAK_POINT == BP_TEST_fail){
return 1;
}
}

}

void __wrap_exit(unsigned int code)
{
dut_set_bp(BP_END);
uint32_t buf = 0;
if (code == CODE_SUCCESS)
{
buf = (1<<30);

}
else if (code == CODE_FAIL)
{
buf = (1<<29);
}
else if (code == CODE_FATAL)
{
buf = (1<<28);
}
buf = buf | (1<<31);
buf = buf | set_log(0);
write_csr(mtval, buf);
GLOBAL_INTERRUPT_ENABLE();
while(1);
return;

}