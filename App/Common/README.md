# common App - the chip-independent product firmware skeleton

The shared skeleton of a product firmware that is not tied to any particular
chip such as S5740 and can run on any RISC-V core. Chip-specific behaviour is
added by overriding the WEAK hooks.

## Differences from VERIF (verification)
| | VERIF/ (verification) | App/CMN/common (product) |
|---|---|---|
| Purpose | hardware verification | real operation |
| Ending | terminates with exit(CODE_SUCCESS) | runs forever in while(1) |
| Environment | simulator | real silicon (shipped in ROM) |

## Structure
```
App/CMN/common/
|-- Init/   app_init.c   - initialization (early->periph->user, WEAK hooks)
|-- Intr/   app_intr.c   - interrupt registration/dispatch (generic table)
`-- Main/   app_main.c   - the product entry point (init + infinite loop)
```

## Flow
```
start.S(_start) -> main -> app_main()
                            |- app_init()              initialization
                            |- app_intr_register_all() interrupts
                            `- for(;;) {               runs forever
                                app_loop_once();       events/tasks
                                app_idle();            low-power wait
                              }
```

## How to extend per chip (WEAK override)
common defaults to no-ops. Implementing a function of the same name in
chip-specific code makes that one win:
```c
/* per chip (e.g. S5740) */
int app_init_periph(void) { /* real peripheral initialization */ return 0; }
int app_loop_once(void)   { /* real event handling */ return 0; }
void app_idle(void)       { __asm__("wfi"); }   /* real low power */
```

## BSP integration
- Wire main in start.S to call app_main() (or call app_main from main)
- Have the BSP trap handler call app_irq_dispatch(irq_id)
- Wire app_irq_enable/disable to the BSP's PLIC/CLIC control

## Chip independence
- No chip-specific features such as RF/L1/IPC mailbox (those belong in
  App/CMN/S5740 and similar)
- Assumes only standard RISC-V plus the BSP (hal) abstraction
- Values such as APP_IRQ_MAX are adjusted per chip with -D
