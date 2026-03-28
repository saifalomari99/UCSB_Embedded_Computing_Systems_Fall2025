#ifndef LFO_H
#define LFO_H

#include "xil_types.h"
#include "interpolate.h"

typedef struct {
    u32 phase;
    u32 phaseIncrement;
    u16 on;
    s16 sample;
    u16 depthQ1_15;
    wave_t wave;
} lfo_t;

void lfo_init(lfo_t *lfo);
void lfo_on(lfo_t *lfo);
void lfo_off(lfo_t *lfo);
void lfo_set_wave(lfo_t *lfo, wave_t wave);
void lfo_set_phase_increment(lfo_t *lfo, u32 phaseIncrement);
void lfo_set_depth_q1_15(lfo_t *lfo, u16 depth);
void lfo_advance(lfo_t *lfo);
s16  lfo_apply(lfo_t *lfo, s16 sample);
u32  lfo_calc_phase_increment_hz(u32 freq_hz);

#endif
