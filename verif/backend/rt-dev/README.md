# The rt-dev backend

rt-dev = the verification engine (Verilator based) the company's original
Verification_SDK ran on.
This adapter layers the chip_platform_v2 seam on top of rt-dev, making the
unified `BACKEND=rt-dev` invocation possible.

## Why an adapter is needed
The test code (VERIF/*) is in the original rt-dev form. However, to invoke it
**through the unified path** with `BACKEND=rt-dev` inside the chip_platform_v2
seam framework, a thin bridge is needed that connects the seam
(master_*/trigger_irq/tick) to the original rt-dev API. (It is unnecessary when
running standalone in the original environment.)

## Files
- `verif_rtdev.h`  : the seam implementation (delegates to rtdev_read/write/irq/tick)
- `run.sh`         : invokes the rt-dev engine (RTDEV=<path>)
- `VERSION`        : version/status

## Differences from chipyard
- trigger_irq: rt-dev = **implemented upstream** (PLIC co-sim works immediately)
  / chipyard = stage 3, unimplemented
- Both are RTL based -> both support CA (get_cycle)

## Running
```
make -f Implementation/Makefile.VERIF src_dir=. F=CLINT SF=rtc BACKEND=rt-dev
```
