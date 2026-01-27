#ifndef CPU_6809_H
#include <stdint.h>
#include <stdbool.h>

bool CPUSetCallbacks(void (*memoryWriteFunction)(uint16_t, uint8_t), uint8_t (*memoryReadFunction)(uint16_t));
void CPUIRQ();
void CPUFIRQ();
void CPUNMI();

void CPUReset();
uint16_t CPUStep();
uint16_t CPUSteps(uint16_t numTicks);
uint16_t CPUGetPC();

#define CPU_6809_H
#endif


