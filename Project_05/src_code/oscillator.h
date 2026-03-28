#ifndef OSCILLATOR_H
#define OSCILLATOR_H

#include "xil_types.h"
#include "interpolate.h"
#include "notes.h"
#include "operator.h"

typedef struct {
    u32 phase;
    u32 phaseIncrement;
    u16 on;
    s16 sample;
    u16 fmDepthQ8_8;
    wave_t wave;
} oscillator_t;

void oscillator_init(oscillator_t *osc);
void oscillator_on(oscillator_t *osc);
void oscillator_off(oscillator_t *osc);
void oscillator_play_note(oscillator_t *osc, operator_t *op, note_t note);
void oscillator_set_wave(oscillator_t *osc, wave_t wave);
void oscillator_set_fm_depth_q8_8(oscillator_t *osc, u16 depth);
s16  oscillator_advance(oscillator_t *osc, operator_t *op);

#endif
