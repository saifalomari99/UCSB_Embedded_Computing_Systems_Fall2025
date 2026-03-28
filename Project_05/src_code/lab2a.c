/*****************************************************************************
* lab2a.c for Lab2A of ECE 153a at UCSB
* Date of the Last Update:  October 23,2014
*****************************************************************************/

#define AO_LAB2A

#include "qpn_port.h"
#include "bsp.h"
#include "lab2a.h"
#include "lcd.h"
#include "audio.h"
#include "notes.h"

// ======================================================================
// Forward declarations
static QState Lab2A_initial(Lab2A *me);
static QState Lab2A_on(Lab2A *me);
static QState Lab2A_welcome(Lab2A *me);
static QState Lab2A_main(Lab2A *me);
static QState Lab2A_edit(Lab2A *me);

static void lab2a_reset_defaults(Lab2A *me);
static void lab2a_apply_audio_params(Lab2A *me);
static void lab2a_select_prev(Lab2A *me);
static void lab2a_select_next(Lab2A *me);
static void lab2a_adjust_param(Lab2A *me, int dir);
static void lab2a_clear_key_voice_map(void);
static void lab2a_stop_all_voices(void);

static uint8_t s_in_main_state = 0;

static const note_t s_key_note_map[] = {
    NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4,
    NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5,
    NOTE_D5, NOTE_E5, NOTE_F5, NOTE_G5,
    NOTE_A5, NOTE_B5
};
#define LAB2A_KEY_COUNT ((int)(sizeof(s_key_note_map) / sizeof(s_key_note_map[0])))
static int s_key_voice_map[LAB2A_KEY_COUNT];


// ======================================================================
//                Instantiate the Active Object
// ======================================================================
Lab2A AO_Lab2A;


// ======================================================================
//                Constructor Function
// ======================================================================
void Lab2A_ctor(void)  {
    Lab2A *me = &AO_Lab2A;
    QActive_ctor(&me->super, (QStateHandler)&Lab2A_initial);
}


// ======================================================================
//                Initial Pseudo-State (Entry Point)
// ======================================================================
static QState Lab2A_initial(Lab2A *me) {
    xil_printf("\n\rlab2a Initialization (0)");

    lab2a_reset_defaults(me);
    lab2a_clear_key_voice_map();

    return Q_TRAN(&Lab2A_welcome);
}


// ======================================================================
//                Top-Level 'On' State
// ======================================================================
static QState Lab2A_on(Lab2A *me) {
    switch (Q_SIG(me)) {
    case Q_ENTRY_SIG: {
        xil_printf("\n\rTop-Level 'On' State\n\r");
        return Q_HANDLED();
    }
    case Q_INIT_SIG: {
        return Q_TRAN(&Lab2A_welcome);
    }
    }

    return Q_SUPER(&QHsm_top);
}


// ======================================================================
//                Welcome State
// ======================================================================
static QState Lab2A_welcome(Lab2A *me) {
    switch (Q_SIG(me)) {
    case Q_ENTRY_SIG: {
        xil_printf("In Welcome state\n\r");
        drawWelcomeScreen();
        return Q_HANDLED();
    }
    case BTN_CENTER: {
        xil_printf("Center pressed, going to main screen\n\r");
        return Q_TRAN(&Lab2A_main);
    }
    }
    return Q_SUPER(&Lab2A_on);
}


// ======================================================================
//                Main Parameter Screen State
// ======================================================================
static QState Lab2A_main(Lab2A *me) {
    switch (Q_SIG(me)) {
    case Q_ENTRY_SIG: {
        xil_printf("In Main state\n\r");
        drawMainStaticLayout();
        drawAllParameterValues(me);
        lab2a_apply_audio_params(me);


        lab2a_clear_key_voice_map();
        s_in_main_state = 1;
        return Q_HANDLED();
    }
    case ENCODER_CLICK: {
        lab2a_reset_defaults(me);
        drawMainStaticLayout();
        drawAllParameterValues(me);
        return Q_HANDLED();
    }
    case SW0_ON: {
        return Q_TRAN(&Lab2A_edit);
    }
    case Q_EXIT_SIG: {
        s_in_main_state = 0;
        // Leaving the piano screen: silence any active voices.
        lab2a_stop_all_voices();
        return Q_HANDLED();
    }
    }
    return Q_SUPER(&Lab2A_on);
}


// ======================================================================
//                Key press hook from BSP
// ======================================================================
void Lab2A_onKeyPressed(int key_index) {
    if (!s_in_main_state) {
        return;
    }
    if (key_index < 0 || key_index >= LAB2A_KEY_COUNT) {
        return;
    }

    // Avoid double-trigger if the same key is already active
    if (s_key_voice_map[key_index] >= 0) {
        return;
    }

    //xil_printf("Playing key %d\r\n", key_index);

    note_t note = s_key_note_map[key_index];
    int voice = audio_note_on_first_free(note);
    if (voice >= 0) {
        s_key_voice_map[key_index] = voice;
    }
}

void Lab2A_onKeyReleased(int key_index) {
    if (!s_in_main_state) {
        return;
    }

    //xil_printf("Key %d released\r\n", key_index);

    if (key_index < 0 || key_index >= LAB2A_KEY_COUNT) {
        return;
    }

    int voice = s_key_voice_map[key_index];
    if (voice >= 0) {
        audio_note_off((u32)voice);
        s_key_voice_map[key_index] = -1;
    }
}


// ======================================================================
//                Helpers
// ======================================================================
static void lab2a_clear_key_voice_map(void) {
    for (int i = 0; i < LAB2A_KEY_COUNT; ++i) {
        s_key_voice_map[i] = -1;
    }
}

static void lab2a_stop_all_voices(void) {
    for (int v = 0; v < AUDIO_MAX_VOICES; ++v) {
        audio_note_off((u32)v);
    }
    lab2a_clear_key_voice_map();
}

static void lab2a_apply_audio_params(Lab2A *me) {
    float ratio_f = (float)me->operator_ratio;
    audio_configure_operator(me->operator_wave, ratio_f);

    float depth = me->operator_depth;
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 10.0f) depth = 10.0f;
    float depth_norm = depth / 10.0f;

    audio_set_fm_depth_norm(depth_norm);
    audio_set_oscillator_wave(me->osc_wave);
}

static void lab2a_reset_defaults(Lab2A *me) {
    me->operator_depth      = OPERATOR_DEFAULT_DEPTH;
    me->operator_wave       = OPERATOR_DEFAULT_WAVE_TYPE;
    me->operator_ratio      = OPERATOR_DEFAULT_RATIO;

    me->osc_wave            = OSC_DEFAULT_WAVE_TYPE;

    me->env_attack_samples  = ENV_DEFAULT_ATTACK_SAMPLES;
    me->env_decay_samples   = ENV_DEFAULT_DECAY_SAMPLES;
    me->env_release_samples = ENV_DEFAULT_RELEASE_SAMPLES;
    me->env_sustain         = ENV_DEFAULT_SUSTAIN;

    me->selected_param      = PARAM_OP_DEPTH;
    me->blink_on            = 0;
}


// ======================================================================
//                Edit State
// ======================================================================
static QState Lab2A_edit(Lab2A *me) {
    switch (Q_SIG(me)) {
    case Q_ENTRY_SIG: {
        xil_printf("In Edit state\n\r");
        drawEditStaticLayout();
        drawAllParameterValues(me);
        me->blink_on = 1;
        drawSelectionCursor(me);
        return Q_HANDLED();
    }
    case TIME_TICK: {
        if (me->blink_on) {
            eraseSelectionCursor(me);   // hide cursor
            me->blink_on = 0;
        } else {
            me->blink_on = 1;
            drawSelectionCursor(me);    // show cursor
        }
        return Q_HANDLED();
    }
    case BTN_UP: {
        eraseSelectionCursor(me);
        lab2a_select_prev(me);
        me->blink_on = 1;
        drawSelectionCursor(me);
        return Q_HANDLED();
    }
    case BTN_DOWN: {
        eraseSelectionCursor(me);
        lab2a_select_next(me);
        me->blink_on = 1;
        drawSelectionCursor(me);
        return Q_HANDLED();
    }
    case BTN_LEFT: {
        lab2a_adjust_param(me, -1);
        me->blink_on = 1;
        lcdUpdateParameterValue(me, me->selected_param);
        drawSelectionCursor(me);
        return Q_HANDLED();
    }
    case BTN_RIGHT: {
        lab2a_adjust_param(me, 1);
        me->blink_on = 1;
        lcdUpdateParameterValue(me, me->selected_param);
        drawSelectionCursor(me);
        return Q_HANDLED();
    }
    case ENCODER_CLICK: {
        lab2a_reset_defaults(me);
        drawAllParameterValues(me);
        me->blink_on = 1;
        drawSelectionCursor(me);
        return Q_HANDLED();
    }
    case SW0_OFF: {
        lab2a_apply_audio_params(me);   // commit edits before returning to play mode
        return Q_TRAN(&Lab2A_main);
    }
    }
    return Q_SUPER(&Lab2A_on);
}


// ======================================================================
//                Private helpers
// ======================================================================
static void lab2a_select_prev(Lab2A *me) {
    if (me->selected_param == 0) {
        me->selected_param = (param_index_t)(PARAM_COUNT - 1);
    } else {
        me->selected_param = (param_index_t)(me->selected_param - 1);
    }
}

static void lab2a_select_next(Lab2A *me) {
    me->selected_param = (param_index_t)((me->selected_param + 1) % PARAM_COUNT);
}

static wave_type_t lab2a_wave_prev(wave_type_t w) {
    if (w == 0) return (wave_type_t)(WAVE_COUNT - 1);
    return (wave_type_t)(w - 1);
}

static wave_type_t lab2a_wave_next(wave_type_t w) {
    return (wave_type_t)((w + 1) % WAVE_COUNT);
}

static float lab2a_clamp_f32(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void lab2a_adjust_param(Lab2A *me, int dir) {
    switch (me->selected_param) {
    case PARAM_OP_DEPTH: {
        float depth = me->operator_depth + (dir * 0.1f);
        if (depth < 0.0f) depth = 0.0f;
        if (depth > 10.0f) depth = 10.0f;
        me->operator_depth = depth;
        break;
    }
    case PARAM_OP_WAVE:
        me->operator_wave = (dir > 0) ? lab2a_wave_next(me->operator_wave)
                                      : lab2a_wave_prev(me->operator_wave);
        break;
    case PARAM_OP_RATIO: {
        int32_t ratio = (int32_t)me->operator_ratio + dir;
        if (ratio < 1) ratio = 1;
        if (ratio > 100) ratio = 100;
        me->operator_ratio = (uint32_t)ratio;
        break;
    }
    case PARAM_OSC_WAVE:
        me->osc_wave = (dir > 0) ? lab2a_wave_next(me->osc_wave)
                                 : lab2a_wave_prev(me->osc_wave);
        break;
    case PARAM_ENV_ATTACK: {
        int32_t v = (int32_t)me->env_attack_samples + (dir * 480);
        if (v < 0) v = 0;
        me->env_attack_samples = (uint32_t)v;
        break;
    }
    case PARAM_ENV_DECAY: {
        int32_t v = (int32_t)me->env_decay_samples + (dir * 480);
        if (v < 0) v = 0;
        me->env_decay_samples = (uint32_t)v;
        break;
    }
    case PARAM_ENV_RELEASE: {
        int32_t v = (int32_t)me->env_release_samples + (dir * 480);
        if (v < 0) v = 0;
        me->env_release_samples = (uint32_t)v;
        break;
    }
    case PARAM_ENV_SUSTAIN: {
        float v = me->env_sustain + (dir * 0.05f);
        me->env_sustain = lab2a_clamp_f32(v, 0.0f, 1.0f);
        break;
    }
    default:
        break;
    }
}
