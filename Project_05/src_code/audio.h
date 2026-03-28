#ifndef AUDIO_H
#define AUDIO_H

#include "xil_types.h"
#include "notes.h"
#include "oscillator.h"
#include "operator.h"
#include "lfo.h"

#define AUDIO_MAX_VOICES 4

typedef struct {
    oscillator_t osc;
    operator_t   op;
    u8           active;
} audio_voice_t;

void audio_init(void);
audio_voice_t *audio_voice_get(u32 index);
int  audio_note_on(u32 index, note_t note);
int  audio_note_on_first_free(note_t note);
void audio_note_off(u32 index);
void audio_set_operator_enabled(u8 enable);
u8   audio_get_operator_enabled(void);
void audio_set_oscillator_wave(wave_t wave);
void audio_set_fm_depth_norm(float fm_depth_norm);
s16  audio_advance(void);
void audio_send_sample(s16 sample);
lfo_t *audio_lfo(void);
void audio_configure_operator(wave_t wave, float ratio);
void audio_configure_oscillator(wave_t wave, float fm_depth_norm);
void audio_configure_lfo(wave_t wave, float depth_norm, u32 freq_hz);
void audio_disable_operator(void);
void audio_log_reset(void);
void audio_dump_duty_log(void);
void audio_log_reset_operator(void);
void audio_dump_operator_log(void);

#endif
