// PLIC_latency.h - CA verification: measure interrupt latency cycles (PMU)
#ifndef PLIC_LATENCY_H
#define PLIC_LATENCY_H
#include <stdint.h>
#define PMU_COUNTER_ADDR(id)   (0x2C00u + ((id) << 4))   // PMUCounter
#define PMU_COUNTER2_ADDR(id)  (0x2C04u + ((id) << 4))   // PMUCounter2
#define PLIC_LAT_RESULT_ADDR   0x80010004u               // result address (absolute)
typedef struct { uint32_t triggerCnt:8, completeCnt:8, accLatency:16; } PMUCounter;
typedef struct { uint32_t maxLatency:16, maxTemp:16; } PMUCounter2;
#define PLIC_IRQ_ID  3
#endif
