#include "oscillator.h"

// Reset oscillator state to defaults.
void oscillator_init(oscillator_t *osc) {
    osc->phase = 0;
    osc->phaseIncrement = 0;
    osc->on = 0;
    osc->sample = 0;
    osc->fmDepthQ8_8 = 0;
    osc->wave = WAVE_SINE;
}

// Enable carrier generation.
void oscillator_on(oscillator_t *osc) {
    osc->on = 1;
}

// Disable carrier generation.
void oscillator_off(oscillator_t *osc) {
    osc->on = 0;
}

// Load a note, configure operator to track its increment, and arm output.
void oscillator_play_note(oscillator_t *osc, operator_t *op, note_t note) {
    oscillator_on(osc);
    osc->phase = 0;
    osc->phaseIncrement = note_to_phase[note];
    operator_play_note(op, osc->phaseIncrement);
}

// Configure FM intensity (Q8.8, 1.0 == 0x0100).
void oscillator_set_fm_depth_q8_8(oscillator_t *osc, u16 depth) {
    osc->fmDepthQ8_8 = depth;
}

// Advance operator + carrier once (called from audio layer).
s16 oscillator_advance(oscillator_t *osc, operator_t *op) {
    if(!osc->on) {
        osc->sample = 0;
        return 0;
    }

    operator_advance(op);
    s32 fm_delta = ((s32)op->sample * (s32)osc->fmDepthQ8_8) << 6;

    osc->phase += osc->phaseIncrement + ((s32)fm_delta);
    osc->sample = interpolate_next_sample(osc->phase, osc->wave);
    return osc->sample;
}
void oscillator_set_wave(oscillator_t *osc, wave_t wave) {
    osc->wave = wave;
}
