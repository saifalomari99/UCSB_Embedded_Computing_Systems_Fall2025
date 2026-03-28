#include "lfo.h"
#include "timer_helpers.h"

// Reset LFO state.
void lfo_init(lfo_t *lfo) {
    lfo->phase = 0;
    lfo->phaseIncrement = 0;
    lfo->on = 0;
    lfo->sample = 0;
    lfo->depthQ1_15 = 0;
    lfo->wave = WAVE_SINE;
}

void lfo_on(lfo_t *lfo) {
    lfo->on = 1;
}

void lfo_off(lfo_t *lfo) {
    lfo->on = 0;
}

void lfo_set_wave(lfo_t *lfo, wave_t wave) {
    lfo->wave = wave;
}

// Configure phase increment directly (e.g., via calc helper).
void lfo_set_phase_increment(lfo_t *lfo, u32 phaseIncrement) {
    lfo->phaseIncrement = phaseIncrement;
}

// Set tremolo depth in Q1.15 (0x4000 ~ 50% swing).
void lfo_set_depth_q1_15(lfo_t *lfo, u16 depth) {
    lfo->depthQ1_15 = depth;
}

// Update LFO phase/sample once per audio tick.
void lfo_advance(lfo_t *lfo) {
    if(!lfo->on) {
        lfo->sample = 0;
        return;
    }

    lfo->sample = interpolate_next_sample(lfo->phase, lfo->wave);
    lfo->phase += lfo->phaseIncrement;
}

// Apply LFO gain to a signed sample.
s16 lfo_apply(lfo_t *lfo, s16 sample) {
    if(!lfo->on || lfo->depthQ1_15 == 0) {
        return sample;
    }

    s32 gain = 32768 + (((s32)lfo->sample * (s32)lfo->depthQ1_15) >> 15);
    s32 scaled = ((s32)sample * gain) >> 15;

    if(scaled > 32767) scaled = 32767;
    else if(scaled < -32768) scaled = -32768;

    return (s16)scaled;
}

// Helper to convert a desired LFO frequency (Hz) into phase increment.
u32 lfo_calc_phase_increment_hz(u32 freq_hz) {
    u64 numerator = ((u64)freq_hz) << 32;
    numerator /= SAMPLE_RATE_HZ;
    return (u32)numerator;
}
