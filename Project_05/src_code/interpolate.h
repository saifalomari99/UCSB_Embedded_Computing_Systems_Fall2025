#ifndef INTERPOLATE_H
#define INTERPOLATE_H

#include "xil_types.h"

typedef enum {
    WAVE_SINE = 0,
    WAVE_TRIANGLE,
    WAVE_SAW,
    WAVE_SQUARE
} wave_t;

s16 interpolate_next_sample(u32 phase, wave_t waveType);

s16 interpolate_sine(u32 phase);

#endif
