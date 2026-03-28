#ifndef INTERRUPT_HELPERS_H
#define INTERRUPT_HELPERS_H

#include "xparameters.h"
#include "xintc.h"

// Globals
extern XIntc   IntcInst;

void interrupts_init(void);

#endif
