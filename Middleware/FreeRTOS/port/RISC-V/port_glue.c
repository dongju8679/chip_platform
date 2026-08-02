/* port_glue.c - the glue layer between FreeRTOS and Platform (TinyRocket
 *               baremetal) - written by us
 *
 * The official port (port.c / portASM.S / portmacro.h) is left exactly as
 * upstream; only this file handles the junction with our baremetal foundation.
 *
 * It owns three things:
 *   1) trap vector switching : mtvec = freertos_risc_v_trap_handler
 *   2) freestanding fill-ins : memset/memcpy/memcmp/strlen (-nostdlib means no libc)
 *   3) FreeRTOS hooks        : malloc failure / stack overflow / configASSERT
 *
 * -- 1) About the trap vector (important) ----------------------------
 * The existing baremetal path is
 *     crt.S -> csrw mtvec, _trap_vector_entry   (Platform/Chipset/TinyRocket/mtvec.S)
 *     _trap_vector_entry -> trap_handler(cause) -> pISR_BASE.time_int_handler()
 * That structure assumes "returning from a trap resumes the very context that
 * was entered" (mtvec.S saves into a 256 B frame above sp and restores it
 * verbatim).
 *
 * RTOS preemption, by definition, must return into a DIFFERENT task's context.
 * That is, the context must be saved not at "sp at the time of the interrupt"
 * but in "each task's TCB (pxCurrentTCB->pxTopOfStack)". mtvec.S knows nothing
 * about pxCurrentTCB, so preemption cannot be wedged into it.
 *
 * So instead of modifying mtvec.S (guaranteeing the existing IP verification
 * path stays intact), only mtvec is swapped to the official FreeRTOS handler
 * right before the scheduler starts.
 *   boot..main()         : crt.S's _trap_vector_entry (unchanged)
 *   vTaskStartScheduler(): freertos_risc_v_trap_handler (context switch capable)
 *
 * The two vectors' register-save conventions are each self-contained:
 *   mtvec.S                     : sp-256 frame, x1~x31 + mepc (full restore under
 *                                 PREEMPTION)
 *   freertos_risc_v_trap_handler: x1,x5~x31 + mepc + mstatus + xCriticalNesting
 *                                 on the task stack, then sp switches to
 *                                 xISRStackTop
 * At the switch point (the mtvec rewrite) no trap is in progress, so the two
 * conventions can never mix.
 */

#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "printf.h"
#include "uart.h"

/* provided by portASM.S */
extern void freertos_risc_v_trap_handler( void );

/*-----------------------------------------------------------*/
/* 1) Trap vector switching                                   */
/*-----------------------------------------------------------*/

/* Replace mtvec with the FreeRTOS trap handler.
 * Call it immediately before vTaskStartScheduler().
 * mode bits [1:0]=00 (direct) - the handler dispatches on mcause itself. */
void vPortInstallFreeRTOSTrapHandler( void )
{
    uintptr_t uxHandler = ( uintptr_t ) freertos_risc_v_trap_handler;

    /* force direct mode (clear the low 2 bits) */
    uxHandler &= ~( uintptr_t ) 0x3u;

    __asm volatile ( "csrw mtvec, %0" ::"r" ( uxHandler ) );
}

/* Restore the original baremetal trap vector (when returning to a path that
 * does not use the scheduler). */
void vPortRestoreBaremetalTrapHandler( void )
{
    extern void _trap_vector_entry( void );
    uintptr_t uxHandler = ( uintptr_t ) _trap_vector_entry;

    uxHandler &= ~( uintptr_t ) 0x3u;
    __asm volatile ( "csrw mtvec, %0" ::"r" ( uxHandler ) );
}

/*-----------------------------------------------------------*/
/* 2) freestanding fill-ins (-nostdlib: no libc/libgcc)       */
/*-----------------------------------------------------------*/
/* Platform/Common/Src/util.c provides only __wrap_memset/__wrap_memcpy (and on
 * top of that carries the rt-dev convention of returning failure outside the
 * ITIM/DTIM range), so without the linker's --wrap option the symbols
 * memset/memcpy are undefined.
 * FreeRTOS (tasks.c, heap_4.c) and GCC require them, so they are supplied
 * here. */

void * memset( void * pvDest, int iValue, size_t xLen )
{
    unsigned char * pucDest = ( unsigned char * ) pvDest;

    while( xLen-- != 0 )
    {
        *pucDest++ = ( unsigned char ) iValue;
    }

    return pvDest;
}

void * memcpy( void * pvDest, const void * pvSrc, size_t xLen )
{
    unsigned char * pucDest = ( unsigned char * ) pvDest;
    const unsigned char * pucSrc = ( const unsigned char * ) pvSrc;

    while( xLen-- != 0 )
    {
        *pucDest++ = *pucSrc++;
    }

    return pvDest;
}

int memcmp( const void * pvA, const void * pvB, size_t xLen )
{
    const unsigned char * pucA = ( const unsigned char * ) pvA;
    const unsigned char * pucB = ( const unsigned char * ) pvB;

    while( xLen-- != 0 )
    {
        if( *pucA != *pucB )
        {
            return ( int ) *pucA - ( int ) *pucB;
        }

        pucA++;
        pucB++;
    }

    return 0;
}

size_t strlen( const char * pcStr )
{
    const char * pcEnd = pcStr;

    while( *pcEnd != '\0' )
    {
        pcEnd++;
    }

    return ( size_t ) ( pcEnd - pcStr );
}

/*-----------------------------------------------------------*/
/* 3) FreeRTOS hooks                                          */
/*-----------------------------------------------------------*/
/* The UART costs about 43,400 cycles (~16 s) per character, so keep even the
 * error-path output as short as possible.
 * Emit just a marker, flush, and halt (the halted PC pins down the cause in
 * simulation). */

static void prvHalt( const char * pcMarker )
{
    uart_puts( pcMarker );
    uart_flush();
    taskDISABLE_INTERRUPTS();

    for( ; ; )
    {
        __asm volatile ( "wfi" );
    }
}

void vAssertCalled( const char * pcFile, unsigned long ulLine )
{
    ( void ) pcFile;
    ( void ) ulLine;
    prvHalt( "\n!ASSERT\n" );
}

void vApplicationMallocFailedHook( void )
{
    /* configTOTAL_HEAP_SIZE exhausted. With the 60K DTIM constraint, suspect
     * this first. */
    prvHalt( "\n!HEAP\n" );
}

void vApplicationStackOverflowHook( TaskHandle_t xTask, char * pcTaskName )
{
    ( void ) xTask;
    ( void ) pcTaskName;
    prvHalt( "\n!STACK\n" );
}
