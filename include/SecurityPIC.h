#ifndef SECURITY_PIC_H
#define SECURITY_PIC_H

#include <stdint.h>

void SecurityPICInit(int machineNumber);
void SecurityPICReset();
bool SecurityPICWrite(uint8_t data, void (*setColFunction)(uint8_t));
uint8_t SecurityPICRead(uint8_t (*getRowFunction)(void));
uint8_t SecurityPICGetScrambler();
void SecurityPICSetLearnedID(int id);
int SecurityPICGetLearnedID();

#endif