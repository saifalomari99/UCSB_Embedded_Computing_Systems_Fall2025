#include <stdint.h>
#include "gpio.h"
#include "audio.h"
#include "timer_helpers.h"
#include "xil_printf.h"

#define KEY_CHANNEL            1U
#define KEY_NOTE_COUNT        12U
#define KEY_TOTAL_COUNT       14U
#define KEY_MASK              ((1U << KEY_TOTAL_COUNT) - 1U)

#define DEBOUNCE_TICKS        50U   // debounce ~1ms (50 * 20us)

#define BASE_NOTE             NOTE_C3
#define OCTAVE_OFFSET_MIN    (-2)
#define OCTAVE_OFFSET_MAX      3

XGpio KeyGpio;

static u32 sStableState = 0;
static u32 sLastChangeTick[KEY_TOTAL_COUNT];
static int8_t sKeyVoiceMap[KEY_NOTE_COUNT];
static int sOctaveOffset = 0;

static note_t key_to_note(u32 keyIndex) {
    int noteIndex = BASE_NOTE + (sOctaveOffset * 12) + (int)keyIndex;
    if(noteIndex < NOTE_C1) noteIndex = NOTE_C1;
    if(noteIndex > NOTE_B6) noteIndex = NOTE_B6;
    return (note_t)noteIndex;
}

static int allocate_voice(void) {
    for(int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        audio_voice_t *voice = audio_voice_get((u32)i);
        if(voice && !voice->active) return i;
    }
    return -1;
}

static void handle_key_press(u32 keyIndex) {
    if(keyIndex < KEY_NOTE_COUNT) {
        if(sKeyVoiceMap[keyIndex] >= 0) {
            return;
        }
        int voice = allocate_voice();
        if(voice < 0) {
            xil_printf("No free voice for key %lu\r\n", (unsigned long)keyIndex);
            return;
        }
        note_t note = key_to_note(keyIndex);
        if(audio_note_on((u32)voice, note) == 0) {
            sKeyVoiceMap[keyIndex] = (int8_t)voice;
        }
        return;
    }

    if(keyIndex == 12U) {
        if(sOctaveOffset < OCTAVE_OFFSET_MAX) {
            sOctaveOffset++;
            xil_printf("Octave up -> offset %d\r\n", sOctaveOffset);
        }
        return;
    }

    if(keyIndex == 13U) {
        if(sOctaveOffset > OCTAVE_OFFSET_MIN) {
            sOctaveOffset--;
            xil_printf("Octave down -> offset %d\r\n", sOctaveOffset);
        }
        return;
    }
}

static void handle_key_release(u32 keyIndex) {
    if(keyIndex >= KEY_NOTE_COUNT) {
        return;
    }
    int voice = sKeyVoiceMap[keyIndex];
    if(voice >= 0) {
        audio_note_off((u32)voice);
        sKeyVoiceMap[keyIndex] = -1;
    }
}

void gpio_init(void) {
    int status = XGpio_Initialize(&KeyGpio, XPAR_AXI_GPIO_KEYS_DEVICE_ID);
    if(status != XST_SUCCESS) {
        xil_printf("KEY GPIO init failed: %d\r\n", status);
        return;
    }

    XGpio_SetDataDirection(&KeyGpio, KEY_CHANNEL, 0xFFFFFFFFU);
    sStableState = XGpio_DiscreteRead(&KeyGpio, KEY_CHANNEL) & KEY_MASK;

    for(u32 i = 0; i < KEY_TOTAL_COUNT; ++i) {
        sLastChangeTick[i] = 0;
        if(i < KEY_NOTE_COUNT) sKeyVoiceMap[i] = -1;
    }

    XGpio_InterruptClear(&KeyGpio, XGPIO_IR_CH1_MASK);
    XGpio_InterruptEnable(&KeyGpio, XGPIO_IR_CH1_MASK);
    XGpio_InterruptGlobalEnable(&KeyGpio);
}

static int debounce_allowed(u32 keyIndex, u32 nowTicks) {
    u32 last = sLastChangeTick[keyIndex];
    if((nowTicks - last) < DEBOUNCE_TICKS) {
        return 0;
    }
    sLastChangeTick[keyIndex] = nowTicks;
    return 1;
}

void gpio_button_isr(void *callback) {
    XGpio *inst = (XGpio *)callback;
    u32 status = XGpio_InterruptGetStatus(inst);
    if((status & XGPIO_IR_CH1_MASK) == 0U) {
        XGpio_InterruptClear(inst, status);
        return;
    }

    u32 rawState = XGpio_DiscreteRead(inst, KEY_CHANNEL) & KEY_MASK;
    u32 changed = sStableState ^ rawState;
    u32 nowTicks = gSystemTicks;

    for(u32 key = 0; key < KEY_TOTAL_COUNT; ++key) {
        u32 mask = 1U << key;
        if((changed & mask) == 0U) continue;
        if(!debounce_allowed(key, nowTicks)) continue;

        if(rawState & mask) {
            sStableState |= mask;
            handle_key_press(key);
        } else {
            sStableState &= ~mask;
            handle_key_release(key);
        }
    }

    XGpio_InterruptClear(inst, status);
}
