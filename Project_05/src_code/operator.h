#ifndef OPERATOR_H
#define OPERATOR_H

#include "interpolate.h"
#include "xil_types.h"

typedef struct {
    u32 phase;
    u32 phaseIncrement;
    u16 on;
    s16 sample;
    u16 ratioQ8_8;
    wave_t wave;
} operator_t;

void operator_init(operator_t *op);
void operator_on(operator_t *op);
void operator_off(operator_t *op);
void operator_play_note(operator_t *op, u32 oscPhaseIncrement);
void operator_set_wave(operator_t *op, wave_t wave);
void operator_set_ratio_q8_8(operator_t *op, u16 ratio);
void operator_advance(operator_t *op);

#define OPERATOR_RATIO_FROM_INT(x)   ((u16)((x) << 8))

#endif
