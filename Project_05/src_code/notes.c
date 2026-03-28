#include "notes.h"
#include "timer_helpers.h"   // SAMPLE_RATE_HZ
#include <math.h>

// C1 frequency reference: 32.703195 Hz (standard equal temperament)
#define C1_FREQ_HZ  32.703195f

uint32_t note_to_phase[NOTE_COUNT];

void note_to_phase_init(void)
{
    const double two32 = 4294967296.0;  // 2^32

    for (int i = 0; i < NOTE_COUNT; ++i) {
        float semitones_from_C1 = (float)i;

        // Equal-tempered scale from C1
        float freq_hz =
            C1_FREQ_HZ * powf(2.0f, semitones_from_C1 / 12.0f);

        // DDS phase increment: freq / fs * 2^32
        double ratio = (double)freq_hz / (double)SAMPLE_RATE_HZ;
        uint32_t phase_inc = (uint32_t)(ratio * two32 + 0.5);

        note_to_phase[i] = phase_inc;
    }
}
