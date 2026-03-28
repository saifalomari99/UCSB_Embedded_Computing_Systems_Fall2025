#include "interpolate.h"
#include "wavetables.h"

s16 interpolate_sine(u32 phase);
static s16 interpolate_triangle(u32 phase);
static s16 interpolate_saw(u32 phase);
static s16 interpolate_square(u32 phase);

s16 interpolate_next_sample(u32 phase, wave_t waveType) {
    switch(waveType) {
    case WAVE_SINE:
        return interpolate_sine(phase);
    case WAVE_TRIANGLE:
        return interpolate_triangle(phase);
    case WAVE_SAW:
        return interpolate_saw(phase);
    case WAVE_SQUARE:
        return interpolate_square(phase);
    default:
        return 0;
    }
}

// static s16 interpolate_sine(u32 phase) {
//     u32 idx = phase >> 24;
//     u32 frac = (phase >> 16) & 0xFFu;
//     u32 idx2 = (idx + 1u) & 0xFFu;

//     s32 y0 = sine_table[idx];
//     s32 y1 = sine_table[idx2];
//     s32 dy = y1 - y0;
//     s32 y = y0 + ((dy * (s32)frac) >> 8);

//     return (s16)y;
// }

s16 interpolate_sine(u32 phase) {
    u32 idx  = phase >> 24;              // top 8 bits: table index 0..255
    u32 frac = (phase >> 16) & 0xFFu;    // next 8 bits: Q0.8 fractional

    u32 i0 = (idx + 255u) & 0xFFu;       // idx - 1
    u32 i1 = idx;                        // idx
    u32 i2 = (idx + 1u) & 0xFFu;         // idx + 1
    u32 i3 = (idx + 2u) & 0xFFu;         // idx + 2

    s32 y0 = sine_table[i0];
    s32 y1 = sine_table[i1];
    s32 y2 = sine_table[i2];
    s32 y3 = sine_table[i3];

    s32 t  = (s32)frac;                  // Q0.8
    s32 t2 = (s32)(((s32)t * t) >> 8);   // Q0.8
    s32 t3 = (s32)(((s32)t2 * t) >> 8);  // Q0.8

    s32 a0 = -y0 + 3*y1 - 3*y2 + y3;
    s32 a1 =  2*y0 - 5*y1 + 4*y2 - y3;
    s32 a2 = -y0 + y2;
    s32 a3 =  2*y1;

    s64 p = a0;
    p = ((p * t)  >> 8) + a1;            // a0*t + a1
    p = ((p * t)  >> 8) + a2;            // (a0*t + a1)*t + a2
    p = ((p * t)  >> 8) + a3;            // ... *t + a3
    p >>= 1;                             // *0.5

    if (p >  32767)  p =  32767;
    if (p < -32768)  p = -32768;

    return (s16)p;
}




static s16 interpolate_triangle(u32 phase) {
    u32 ramp = phase >> 16;                           // 0 .. 65535
    u32 folded = (ramp < 32768u) ? ramp : (65535u - ramp);
    s32 sample = ((s32)folded << 1) - 32767;          // -32767 .. 32767
    return (s16)sample;
}

static s16 interpolate_saw(u32 phase) {
    s32 sample = (s32)(phase >> 16);                  // 0 .. 65535
    sample -= 32768;                                  // -32768 .. 32767
    return (s16)sample;
}

static s16 interpolate_square(u32 phase) {
    return (phase & 0x80000000u) ? 32767 : -32768;
}
