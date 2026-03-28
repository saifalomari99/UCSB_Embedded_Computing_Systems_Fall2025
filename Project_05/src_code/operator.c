#include "operator.h"

// Reset operator state; call once at startup.
void operator_init(operator_t *op) {
    op->phase = 0;
    op->phaseIncrement = 0;
    op->on = 0;
    op->sample = 0;
    op->ratioQ8_8 = OPERATOR_RATIO_FROM_INT(1);
    op->wave = WAVE_SINE;
}

void operator_on(operator_t *op) {
    op->on = 1;
}

void operator_off(operator_t *op) {
    op->on = 0;
}

void operator_set_ratio_q8_8(operator_t *op, u16 ratio) {
    op->ratioQ8_8 = ratio;
}

void operator_set_wave(operator_t *op, wave_t wave) {
    op->wave = wave;
}

// Compute operator increment from carrier increment (Q8.8 ratio).
void operator_play_note(operator_t *op, u32 oscPhaseIncrement) {
    op->phase = 0;
    u64 scaled = (u64)oscPhaseIncrement * (u64)op->ratioQ8_8;
    op->phaseIncrement = (u32)(scaled >> 8);
}

void operator_advance(operator_t *op) {
    if(!op->on) {
        op->sample = 0;
        return;
    }
    op->sample = interpolate_next_sample(op->phase, op->wave);
    op->phase += op->phaseIncrement;
}
