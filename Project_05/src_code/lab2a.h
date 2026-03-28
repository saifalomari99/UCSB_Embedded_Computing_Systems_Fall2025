/*****************************************************************************
* lab2a.h for Lab2A of ECE 153a at UCSB
* Date of the Last Update:  October 23,2014
*****************************************************************************/

#ifndef lab2a_h
#define lab2a_h

#include <stdint.h>
#include "qpn_port.h"
#include "interpolate.h"

typedef enum {
    TIME_TICK = Q_USER_SIG,   /* periodic tick from timer ISR */
    ENCODER_UP,
    ENCODER_DOWN,
    ENCODER_CLICK,
    BTN_CENTER,               /* center push button */
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    SW0_ON,
    SW0_OFF,
    MAX_SIG
} Lab2ASignals;

typedef wave_t wave_type_t;
#define WAVE_COUNT 4

typedef enum {
    PARAM_OP_DEPTH = 0,
    PARAM_OP_WAVE,
    PARAM_OP_RATIO,
    PARAM_OSC_WAVE,
    PARAM_ENV_ATTACK,
    PARAM_ENV_DECAY,
    PARAM_ENV_RELEASE,
    PARAM_ENV_SUSTAIN,
    PARAM_COUNT
} param_index_t;

/* Parameter defaults */
#define OPERATOR_DEFAULT_DEPTH       0.0f

#define OPERATOR_DEFAULT_WAVE_TYPE   WAVE_SINE
#define OPERATOR_DEFAULT_RATIO       1      /* integer 1 to 100 */

#define OSC_DEFAULT_WAVE_TYPE        WAVE_SINE

/* Envelope stored internally as sample counts */
#define ENV_DEFAULT_ATTACK_SAMPLES   4800u
#define ENV_DEFAULT_DECAY_SAMPLES    9600u
#define ENV_DEFAULT_RELEASE_SAMPLES  9600u
#define ENV_DEFAULT_SUSTAIN          0.80f   /* 0.00 to 1.00 */

typedef struct Lab2ATag {
    QActive super;

    /* Operator */
    float        operator_depth;    /* 0.0 to 10.0 */
    wave_type_t  operator_wave;
    uint32_t     operator_ratio;    /* 1 to 100 */

    /* Oscillator */
    wave_type_t  osc_wave;

    /* Envelope, stored as 32 bit unsigned sample counts */
    uint32_t     env_attack_samples;
    uint32_t     env_decay_samples;
    uint32_t     env_release_samples;
    float        env_sustain;       /* 0.00 to 1.00 */

    /* Editing state */
    param_index_t selected_param;
    uint8_t       blink_on;

} Lab2A;

extern Lab2A AO_Lab2A;

void Lab2A_ctor(void);
void GpioHandler(void *CallbackRef);
void TwistHandler(void *CallbackRef);
void Lab2A_onKeyPressed(int key_index);
void Lab2A_onKeyReleased(int key_index);

#endif  
