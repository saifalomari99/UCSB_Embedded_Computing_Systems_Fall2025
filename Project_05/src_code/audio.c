#include "audio.h"
#include "timer_helpers.h"
#include "xil_printf.h"

static audio_voice_t gVoices[AUDIO_MAX_VOICES];
static lfo_t gLfo;
static u32 gDutyLog[2048];
static u32 gDutyLogIndex = 0;
static s16 gOpLog[2048];
static u32 gOpLogIndex = 0;
static s16 gLastOutput = 0;
static u8  gOperatorEnabled = 1;

static inline s32 divide_by_three_s32(s32 value) {
    s64 prod = (s64)value * 0xAAAAAAABLL;
    return (s32)(prod >> 33);
}

// Initialize every voice in the synth.
void audio_init(void) {
    for(int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        audio_voice_t *voice = &gVoices[i];
        oscillator_init(&voice->osc);
        operator_init(&voice->op);
        voice->active = 0;
    }
    lfo_init(&gLfo);
    gLastOutput = 0;
    gOperatorEnabled = 1;
}

audio_voice_t *audio_voice_get(u32 index) {
    if(index >= AUDIO_MAX_VOICES) return NULL;
    return &gVoices[index];
}

lfo_t *audio_lfo(void) {
    return &gLfo;
}

void audio_configure_operator(wave_t wave, float ratio) {
    if(ratio < 0.0f) ratio = 0.0f;
    for(int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        operator_set_wave(&gVoices[i].op, wave);
        u16 ratio_q8_8 = (u16)(ratio * 256.0f + 0.5f);
        operator_set_ratio_q8_8(&gVoices[i].op, ratio_q8_8);
    }
}

void audio_configure_oscillator(wave_t wave, float fm_depth_norm) {
    if(fm_depth_norm < 0.0f) fm_depth_norm = 0.0f;
    if(fm_depth_norm > 1.0f) fm_depth_norm = 1.0f;
    const float max_q8_8 = 4096.0f; // allow up to 16.0 depth in Q8.8
    u16 depth_q8_8 = (u16)(fm_depth_norm * max_q8_8 + 0.5f);
    for(int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        oscillator_set_wave(&gVoices[i].osc, wave);
        oscillator_set_fm_depth_q8_8(&gVoices[i].osc, depth_q8_8);
    }
}

void audio_configure_lfo(wave_t wave, float depth_norm, u32 freq_hz) {
    if(depth_norm < 0.0f) depth_norm = 0.0f;
    if(depth_norm > 1.0f) depth_norm = 1.0f;
    u16 depth_q1_15 = (u16)(depth_norm * 32767.0f + 0.5f);
    lfo_set_wave(&gLfo, wave);
    lfo_set_depth_q1_15(&gLfo, depth_q1_15);
    lfo_set_phase_increment(&gLfo, lfo_calc_phase_increment_hz(freq_hz));
}

void audio_disable_operator(void) {
    for(int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        operator_off(&gVoices[i].op);
    }
}

void audio_log_reset(void) {
    gDutyLogIndex = 0;
}

void audio_dump_duty_log(void) {
    xil_printf("Duty log count: %lu\r\n", (unsigned long)gDutyLogIndex);
    for(u32 i = 0; i < gDutyLogIndex; ++i) {
        xil_printf("%lu\r\n", (unsigned long)gDutyLog[i]);
    }
}

void audio_log_reset_operator(void) {
    gOpLogIndex = 0;
}

void audio_dump_operator_log(void) {
    xil_printf("Operator log count: %lu\r\n", (unsigned long)gOpLogIndex);
    for(u32 i = 0; i < gOpLogIndex; ++i) {
        xil_printf("%d\r\n", (int)gOpLog[i]);
    }
}

void audio_set_operator_enabled(u8 enable) {
    gOperatorEnabled = enable ? 1u : 0u;
}

u8 audio_get_operator_enabled(void) {
    return gOperatorEnabled;
}

void audio_set_oscillator_wave(wave_t wave) {
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        oscillator_set_wave(&gVoices[i].osc, wave);
    }
}

void audio_set_fm_depth_norm(float fm_depth_norm) {
    if (fm_depth_norm < 0.0f) fm_depth_norm = 0.0f;
    if (fm_depth_norm > 1.0f) fm_depth_norm = 1.0f;
    const float max_q8_8 = 4096.0f; // matches audio_configure_oscillator
    u16 depth_q8_8 = (u16)(fm_depth_norm * max_q8_8 + 0.5f);
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        oscillator_set_fm_depth_q8_8(&gVoices[i].osc, depth_q8_8);
    }
}

int audio_note_on_first_free(note_t note) {
    for(u32 i = 0; i < AUDIO_MAX_VOICES; ++i) {
        if(!gVoices[i].active) {
            if (audio_note_on(i, note) == 0) {
                return (int)i;
            }
            return -1;
        }
    }
    // All voices busy: reuse voice 0 to match previous behaviour
    if (audio_note_on(0u, note) == 0) {
        return 0;
    }
    return -1;
}

int audio_note_on(u32 index, note_t note) {
    audio_voice_t *voice = audio_voice_get(index);
    if(!voice) return -1;

    // Operator stays on; depth (FM amount) is controlled separately.
    operator_on(&voice->op);
    oscillator_play_note(&voice->osc, &voice->op, note);
    voice->active = 1;
    return 0;
}

void audio_note_off(u32 index) {
    audio_voice_t *voice = audio_voice_get(index);
    if(!voice) return;

    oscillator_off(&voice->osc);
    operator_off(&voice->op);
    voice->active = 0;
}

// Step every active voice once and mix the results, then apply global LFO.
s16 audio_advance(void) {
    s32 mix = 0;
    int activeCount = 0;

    for(int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        audio_voice_t *voice = &gVoices[i];
        if(!voice->active) continue;

        s16 sample = oscillator_advance(&voice->osc, &voice->op);
        if(i == 0 && gOpLogIndex < (sizeof(gOpLog)/sizeof(gOpLog[0]))) {
            gOpLog[gOpLogIndex++] = voice->op.sample;
        }
        mix += sample;
        activeCount++;
    }

    if(activeCount == 0) {
        if(gLastOutput != 0) {
            gLastOutput -= gLastOutput >> 4;    // exponential decay to suppress pops
        }
        return gLastOutput;
    }

    s32 scaled = mix;
    switch(activeCount) {
    case 1:
        scaled >>= 1;    // leave headroom so post-LFO gain won't clip
        break;
    case 2:
        scaled >>= 1;
        break;
    case 3:
        scaled = divide_by_three_s32(scaled);
        break;
    default:    // 4 voices
        scaled >>= 2;
        break;
    }

    // Apply LFO gain and clamp the final result.
    lfo_advance(&gLfo);
    s16 modulated = lfo_apply(&gLfo, (s16)scaled);
    if(modulated > 32767) modulated = 32767;
    else if(modulated < -32768) modulated = -32768;
    gLastOutput = modulated;
    return modulated;
}

// Convert latest mixed sample into a PWM duty cycle centered at 1024 +/- 512.
void audio_send_sample(s16 sample) {
    s32 v = sample >> 6;
    u32 duty = (u32)(1024 + v);
    if(gDutyLogIndex < (sizeof(gDutyLog)/sizeof(gDutyLog[0]))) {
        gDutyLog[gDutyLogIndex++] = duty;
    }
    pwm_set_duty(duty);
}
