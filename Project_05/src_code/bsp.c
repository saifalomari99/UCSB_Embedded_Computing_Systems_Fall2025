//*****************************************************************************
// bsp.c for Lab2B
//
// ===========================================================================

// ---------------------------------------- header files
#include "qpn_port.h"
#include "bsp.h"
#include "lab2a.h"                    // includes the encoder Qstates
#include "xintc.h"
#include "xil_exception.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xspi_l.h"
#include "xgpio_l.h"
#include "xspi.h"
#include "lcd.h"
#include "xtmrctr.h"

// add these:
#include "audio.h"
#include "notes.h"
#include "timer_helpers.h"
#include "interrupt_helpers.h"
#include "gpio.h"

extern void Lab2A_onKeyPressed(int key_index);
extern void Lab2A_onKeyReleased(int key_index);

// ----------------------------------------- I/O Devices from xparameters.h
// --- Encoder GPIO (canonical for ENCODER block)
#define ENC_GPIO_DEVICE_ID   XPAR_GPIO_0_DEVICE_ID
#define ENC_GPIO_CHANNEL     1
// --- Interrupt controller
#define INTC_DEVICE_ID       XPAR_INTC_0_DEVICE_ID
#define ENC_IRQ_VEC_ID       XPAR_INTC_0_GPIO_0_VEC_ID
// ---- Timer
// Timer 0 is reserved for PWM; timer 1 is used for the audio interrupt tick.
// Use a dedicated AXI Timer 2 for the BSP free-running counter so it does not
// clash with PWM on timer 0.
#if defined(XPAR_TMRCTR_2_DEVICE_ID)
#define TIMER_DEV_ID   XPAR_TMRCTR_2_DEVICE_ID
#elif defined(XPAR_AXI_TIMER_2_DEVICE_ID)
#define TIMER_DEV_ID   XPAR_AXI_TIMER_2_DEVICE_ID
#else
#error "AXI Timer 2 not defined. Add AXI Timer 2 to the design and regenerate BSP."
#endif
#define TIMER_CH       0

// --- Push buttons GPIO
#define BTN_GPIO_DEVICE_ID  XPAR_AXI_GPIO_BTN_DEVICE_ID   // from prior lab
#define BTN_GPIO_CHANNEL    1
// --- Extra keys GPIO (Pmod JC/JD)
#define KEYS_GPIO_DEVICE_ID XPAR_AXI_GPIO_KEYS_DEVICE_ID
#define KEYS_GPIO_CHANNEL   1
#define KEYS_COUNT          14
#define KEYS_ALL_MASK       0x3FFFu   // bits [13:0]


// ----------------------------------------- define parameters
// --- JA[2:0] bit mapping: A=JA[1], B=JA[0], Push=JA[2]
#define AB_A_BIT  1u
#define AB_B_BIT  0u
#define PB_BIT    2u
#define AB_A_MASK (1u << AB_A_BIT)
#define AB_B_MASK (1u << AB_B_BIT)
#define PB_MASK   (1u << PB_BIT)
// ---- Timer
#define TICK_PERIOD_US 1000000U   // 1 s per tick. Change to 500000U for 0.5 s.
// ---- Push button:
#define BTN_UP_MASK     (1u << 0)   // btn[0]
#define BTN_LEFT_MASK   (1u << 1)   // btn[1]
#define BTN_RIGHT_MASK  (1u << 2)   // btn[2]
#define BTN_DOWN_MASK   (1u << 3)   // btn[3]
#define BTN_CENTER_MASK (1u << 4)   // btn[4]
#define BTN_ALL_MASK (BTN_UP_MASK|BTN_LEFT_MASK|BTN_RIGHT_MASK|BTN_DOWN_MASK|BTN_CENTER_MASK)
#define BTN_DEBOUNCE_US 30000u
// --- Switches GPIO
#define SW_GPIO_DEVICE_ID  XPAR_AXI_GPIO_SW_DEVICE_ID
#define SW_GPIO_CHANNEL    1
#define SW0_MASK           (1u << 0)



// ----------------------------------------- globals
static XGpio gpio_enc;                                    // AXI GPIO instance for encoder
// ---- lcd
static XSpi  s_spi;   // AXI Quad SPI instance for LCD
static XGpio s_dc;    // AXI GPIO for D/C line
// ---- timer
static XTmrCtr sys_timer;
// ---- buttons
static XGpio gpio_btn;
static u32 g_btn_prev = 0;
static XGpio gpio_keys;
static u32 g_keys_prev = 0;
// ---- switches
static XGpio gpio_sw;
static u32 g_sw_prev = 0;

// ------------------------------- local encoder state
typedef enum
{
	S11_IDLE  = 0,
	S01_RIGHT = 1,
	S00_RIGHT = 2,
	S10_RIGHT = 3,
	S10_LEFT  = 4,
	S00_LEFT  = 5,
	S01_LEFT  = 6
} enc_state_t;
static volatile enc_state_t enc_state = S11_IDLE;                  // start in 11 idle state
static volatile unsigned char pb_last = 0;                      // last sampled push level
//static volatile int pb_db_cnt = 0;                              // simple debounce countdown


static int g_btn_msg_ticks = 0;   // 0 means hidden, otherwise counts TIME_TICKs visible


// ============================== Functions
// Forward declarations for timer helpers used by time_tick_poll
static inline u32 tmr_now(void);
static inline u32 tmr_elapsed_ticks(u32 start);
static inline u32 ticks_to_us(u32 ticks);
void debounceInterrupt(); // Write This function
static void enc_isr(void *cb);
static void buttons_poll(void);
static void keys_poll(void);
static void switches_poll(void);
static int btn_ok_after(u32 last, u32 now_ticks);
static void time_tick_poll(void);




// ============================================== BSP Init ======================================================
/*..........................................................................*/
void BSP_init(void) {


    xil_printf("Audio step 1: timer_init\r\n");
    timer_init();                                     /// The problem is here
    interrupts_init();                                // single INTC for timer + GPIO

         xil_printf("Audio step 3: note_to_phase_init\r\n");
         note_to_phase_init();
         xil_printf("Audio step 4: audio_init\r\n");
         audio_init();

         xil_printf("Audio step 5: config operator\r\n");
         audio_configure_operator(WAVE_TRIANGLE, 2.0f);
         xil_printf("Audio step 6: config osc\r\n");
         audio_configure_oscillator(WAVE_SINE, 0.0f);
         xil_printf("Audio step 7: config lfo\r\n");
         audio_configure_lfo(WAVE_SINE, 0.8f, 20);
         xil_printf("Audio step 8: lfo on/off\r\n");
         lfo_on(audio_lfo());
         lfo_off(audio_lfo());

         xil_printf("Audio step 9: start timers\r\n");
         resample = 0;

         xil_printf("Audio step 10: timers started\r\n");





	// =================== Encoder ============================
    // 1) Initialize AXI GPIO for encoder pins as inputs
    //int st;
    XGpio_Initialize(&gpio_enc, ENC_GPIO_DEVICE_ID);   // returns 0 on success
    XGpio_SetDataDirection(&gpio_enc, ENC_GPIO_CHANNEL, 0xFFFFFFFFu); // all inputs

    // 2) Initialize INTC and connect the encoder ISR
    XIntc_Connect(&IntcInst, ENC_IRQ_VEC_ID, (XInterruptHandler)enc_isr, NULL);

    // 3) Enable the encoder interrupt vector on the shared INTC
    XIntc_Enable(&IntcInst, ENC_IRQ_VEC_ID);

    // 4) Enable AXI GPIO interrupts for bits A, B, and push
    XGpio_InterruptGlobalEnable(&gpio_enc);
    XGpio_InterruptEnable(&gpio_enc, AB_A_MASK | AB_B_MASK | PB_MASK);

    xil_printf("Encoder init ready\r\n");


    // =================== LCD init ===================
     // 1) D/C GPIO as output (channel 1, 1 bit wide)
     XGpio_Initialize(&s_dc, XPAR_SPI_DC_DEVICE_ID);
     XGpio_SetDataDirection(&s_dc, 1, 0x0);

     // 2) SPI init
     XSpi_Config *cfg = XSpi_LookupConfig(XPAR_SPI_DEVICE_ID);
     XSpi_CfgInitialize(&s_spi, cfg, cfg->BaseAddress);
     XSpi_Reset(&s_spi);

     // 3) Enable master mode and release TRANS_INHIBIT
     u32 cr = XSpi_GetControlReg(&s_spi);
     XSpi_SetControlReg(&s_spi,
         (cr | XSP_CR_ENABLE_MASK | XSP_CR_MASTER_MODE_MASK) & (~XSP_CR_TRANS_INHIBIT_MASK));

     // 4) Select slave 0 (SS0 active)
     XSpi_SetSlaveSelectReg(&s_spi, ~0x01);

     // 5) Power up LCD controller
     initLCD();
     clrScr();                        // optional clear
     xil_printf("LCD init done\r\n");


     // ==================== Timer =====================
    XTmrCtr_Initialize(&sys_timer, TIMER_DEV_ID);

    // Free-run, auto-reload, down-count
    XTmrCtr_SetOptions(&sys_timer, TIMER_CH,
        XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);

    // Reload value = max so it free-runs (wraps) for a long time
    XTmrCtr_SetResetValue(&sys_timer, TIMER_CH, 0xFFFFFFFFu);

    XTmrCtr_Start(&sys_timer, TIMER_CH);
    xil_printf("Timer started\r\n");

     // =================== Buttons init ===================
     XGpio_Initialize(&gpio_btn, BTN_GPIO_DEVICE_ID);
     XGpio_SetDataDirection(&gpio_btn, BTN_GPIO_CHANNEL, 0xFFFFFFFFu); // inputs
     g_btn_prev = XGpio_DiscreteRead(&gpio_btn, BTN_GPIO_CHANNEL) & BTN_ALL_MASK;
     xil_printf("Buttons init ready\r\n");

     // =================== Keys init (Pmod JC/JD) ===================
     XGpio_Initialize(&gpio_keys, KEYS_GPIO_DEVICE_ID);
     XGpio_SetDataDirection(&gpio_keys, KEYS_GPIO_CHANNEL, 0xFFFFFFFFu); // inputs
     g_keys_prev = XGpio_DiscreteRead(&gpio_keys, KEYS_GPIO_CHANNEL) & KEYS_ALL_MASK;
     xil_printf("Keys init ready\r\n");

     // =================== Switches init ===================
     XGpio_Initialize(&gpio_sw, SW_GPIO_DEVICE_ID);
     XGpio_SetDataDirection(&gpio_sw, SW_GPIO_CHANNEL, 0xFFFFFFFFu); // inputs
     g_sw_prev = XGpio_DiscreteRead(&gpio_sw, SW_GPIO_CHANNEL) & SW0_MASK;
     xil_printf("Switches init ready\r\n");









     XTmrCtr_Start(&TimerIrpt, 0);
     XTmrCtr_Start(&TimerPWM, 0);




}







// ============================================= QFSM Top States =============================================
extern XTmrCtr TimerIrpt;
extern XTmrCtr TimerPWM;
// extern volatile u16 resample;

void QF_onStartup(void) {   /* entered with interrupts locked */
    xil_printf("\n\rQF_onStartup\n\r");


}



void QF_onIdle(void) {                         /* entered with interrupts locked */

    QF_INT_UNLOCK();                       /* unlock interrupts */

    static int idle_cnt = 0;
    if ((idle_cnt++ % 100000) == 0) {
        xil_printf(".");
    }

    if(resample > 0) {
        resample--;
        s16 mix = audio_advance();
        audio_send_sample(mix);

    }
    
    //timer_heartbeat_poll();           // prints ~once per second
    time_tick_poll();                   // posts TIME_TICK every TICK_PERIOD_US
    buttons_poll();
    keys_poll();
    switches_poll();

}

/* Q_onAssert is called only when the program encounters an error*/
/*..........................................................................*/
void Q_onAssert(char const Q_ROM * const Q_ROM_VAR file, int line) {
    (void)file;                                   /* name of the file that throws the error */
    (void)line;                                   /* line of the code that throws the error */
    QF_INT_LOCK();
    printDebugLog();
    for (;;) {
    }
}
// ================================================ END of QFSM States =============================================




/* Interrupt handler functions here.  Do not forget to include them in lab2a.h!
To post an event from an ISR, use this template:
QActive_postISR((QActive *)&AO_Lab2A, SIGNALHERE);
Where the Signals are defined in lab2a.h  */
// ---- Read A and B as a 2 bit value {B,A} = (b<<1)|a, returns 0..3
static inline unsigned enc_read_ab(void) {
    unsigned v = XGpio_DiscreteRead(&gpio_enc, ENC_GPIO_CHANNEL);
    unsigned b = (v & AB_A_MASK) ? 1u : 0u;
    unsigned a = (v & AB_B_MASK) ? 1u : 0u;
    return ((b << 1) | a) & 0x3u;
}


// ===== Soft tick generator: posts TIME_TICK every TICK_PERIOD_US =====
static void time_tick_poll(void) {
    static u32 t0 = 0;
    static int inited = 0;

    if (!inited) { t0 = tmr_now(); inited = 1; return; }

    u32 dt_ticks = tmr_elapsed_ticks(t0);
    u32 dt_us    = ticks_to_us(dt_ticks);

    if (dt_us >= TICK_PERIOD_US) {
        // post one TIME_TICK to the AO
        QActive_postISR((QActive *)&AO_Lab2A, TIME_TICK);
        t0 = tmr_now();
    }



    // auto hide the button banner after 2 ticks
    extern void clearButtonMessage(void);   // from lcd.h
    if (g_btn_msg_ticks > 0) {
        g_btn_msg_ticks++;
        if (g_btn_msg_ticks >= 270000) {
            clearButtonMessage();
            g_btn_msg_ticks = 0;
        }
    }
}


/* ============================ Encoder ISR =============================== */
// Posts ENCODER_UP on CW, ENCODER_DOWN on CCW, ENCODER_CLICK on debounced push
static void enc_isr(void *cb) {
    (void)cb;

    // Latch and clear pending early
    unsigned pending = XGpio_InterruptGetStatus(&gpio_enc);
    XGpio_InterruptClear(&gpio_enc, pending);

    // Nothing to do
    if ((pending & (AB_A_MASK | AB_B_MASK | PB_MASK)) == 0u) return;

    // Sample A/B and push
    unsigned ab = enc_read_ab();
    unsigned char pb = (XGpio_DiscreteRead(&gpio_enc, ENC_GPIO_CHANNEL) & PB_MASK) ? 1u : 0u;



    // ---- Saif's Encoder FSM
    switch (enc_state) {
    case S11_IDLE:
        if      (ab == 0b01) enc_state = S01_RIGHT;        // possible CW path
        else if (ab == 0b10) enc_state = S10_LEFT;        // possible CCW path
        break;
    // ---------- right direction
    case S01_RIGHT:
        if      (ab == 0b00) enc_state = S00_RIGHT;
        else if (ab == 0b11) enc_state = S11_IDLE;
        break;
    case S00_RIGHT:
        if      (ab == 0b10) enc_state = S10_RIGHT;
        else if (ab == 0b01) enc_state = S01_RIGHT;
        break;
    case S10_RIGHT:
        if      (ab == 0b00) enc_state = S00_RIGHT;
        else if (ab == 0b11) { enc_state = S11_IDLE; QActive_postISR((QActive *)&AO_Lab2A, ENCODER_UP ); }
        break;
    // ------------ Left direction
    case S10_LEFT:
        if      (ab == 0b00) enc_state = S00_LEFT;
        else if (ab == 0b11) enc_state = S11_IDLE;
        break;
    case S00_LEFT:
        if      (ab == 0b10) enc_state = S10_LEFT;
        else if (ab == 0b01) enc_state = S01_LEFT;
        break;
    case S01_LEFT:
        if      (ab == 0b00) enc_state = S00_LEFT;
        else if (ab == 0b11) { enc_state = S11_IDLE; QActive_postISR((QActive *)&AO_Lab2A, ENCODER_DOWN ); }
        break;
    }


    // ---- Push button: post on rising edge (simple and reliable)
    if (pb && !pb_last) {
        QActive_postISR((QActive *)&AO_Lab2A, ENCODER_CLICK);
    }
    pb_last = pb;



}


// read current counter value (channel 0)
static inline u32 tmr_now(void) {
    //return XTmrCtr_GetValue(&sys_timer, 0);
    return XTmrCtr_GetValue(&sys_timer, TIMER_CH );
}

static inline u32 tmr_elapsed_ticks(u32 start) {
    return (u32)(start - tmr_now());
}
static inline u32 ticks_to_us(u32 ticks) {
    return (u32)(ticks / (XPAR_TMRCTR_0_CLOCK_FREQ_HZ / 1000000U));
}

// non-blocking heartbeat that prints about once per second
//static void timer_heartbeat_poll(void) {
//    static u32 t0 = 0;
//    static int  inited = 0;
//    if (!inited) { t0 = tmr_now(); inited = 1; return; }
//
//    u32 dt_ticks = tmr_elapsed_ticks(t0);
//    if (ticks_to_us(dt_ticks) >= 1000000U) {
//        xil_printf("[HB] ~1 s elapsed (ticks=%u)\r\n", (unsigned)dt_ticks);
//        t0 = tmr_now(); // re-arm for next second
//    }
//}




static int btn_ok_after(u32 last, u32 now_ticks) {
    // timer is down counting
    return ticks_to_us((u32)(last - now_ticks)) > BTN_DEBOUNCE_US ? 1 : 0;
}

// static void keys_poll(void) {
//     static u32 t_last[KEYS_COUNT] = {0};

//     u32 now = tmr_now();
//     u32 v = XGpio_DiscreteRead(&gpio_keys, KEYS_GPIO_CHANNEL) & KEYS_ALL_MASK;

//     // If the external buttons are wired active-low, invert here:
//     // v ^= KEYS_ALL_MASK;

//     u32 prev = g_keys_prev;
//     u32 next_prev = prev;

//     // Only accept a new stable state after debounce so we don't lose a release edge.
//     for (int i = 0; i < KEYS_COUNT; ++i) {
//         u32 mask = (1u << i);
//         int was_pressed = (prev & mask) ? 1 : 0;
//         int is_pressed  = (v & mask) ? 1 : 0;

//         if (!was_pressed && is_pressed && btn_ok_after(t_last[i], now)) {
//             xil_printf("[KEY] keys[%d] pressed\r\n", i);
//             Lab2A_onKeyPressed(i);
//             t_last[i] = now;
//             next_prev |= mask;   // accept new stable pressed state
//         }

//         if (was_pressed && !is_pressed && btn_ok_after(t_last[i], now)) {
//             xil_printf("[KEY] keys[%d] released\r\n", i);
//             Lab2A_onKeyReleased(i);
//             t_last[i] = now;
//             next_prev &= ~mask;  // accept new stable released state
//         }
//     }

//     g_keys_prev = next_prev;
// }
static void keys_poll(void) {
    static u32 t_last[KEYS_COUNT] = {0};

    u32 now = tmr_now();
    u32 v   = XGpio_DiscreteRead(&gpio_keys, KEYS_GPIO_CHANNEL) & KEYS_ALL_MASK;

    // External keys are wired active-low: invert so pressed = 1
    v ^= KEYS_ALL_MASK;

    u32 rising  = v & ~g_keys_prev;
    u32 falling = (~v) & g_keys_prev;
    g_keys_prev = v;

    for (int i = 0; i < KEYS_COUNT; ++i) {
        u32 mask = (1u << i);

        if ((rising & mask) && btn_ok_after(t_last[i], now)) {
            //xil_printf("[KEY] keys[%d] pressed\r\n", i);
            Lab2A_onKeyPressed(i);
            t_last[i] = now;
        }

        if ((falling & mask) && btn_ok_after(t_last[i], now)) {
            //xil_printf("[KEY] keys[%d] released\r\n", i);
            Lab2A_onKeyReleased(i);
            t_last[i] = now;
        }
    }
}


static void buttons_poll(void) {
    static u32 t_last_up = 0, t_last_left = 0, t_last_right = 0, t_last_down = 0, t_last_ctr = 0;

    u32 now = tmr_now();
    u32 v = XGpio_DiscreteRead(&gpio_btn, BTN_GPIO_CHANNEL) & BTN_ALL_MASK;

    // rising edges
    u32 rising = v & ~g_btn_prev;
    g_btn_prev = v;

    if ((rising & BTN_UP_MASK) && btn_ok_after(t_last_up, now)) {
        clearButtonMessage();
        xil_printf("[BTN] UP     pressed\r\n");
        g_btn_msg_ticks = 1;              // start auto-hide
        t_last_up = now;
        QActive_post((QActive *)&AO_Lab2A, BTN_UP);
    }

    if ((rising & BTN_LEFT_MASK) && btn_ok_after(t_last_left, now)) {
        clearButtonMessage();
        xil_printf("[BTN] LEFT   pressed\r\n");
        g_btn_msg_ticks = 1;
        t_last_left = now;
        QActive_post((QActive *)&AO_Lab2A, BTN_LEFT);
    }

    if ((rising & BTN_RIGHT_MASK) && btn_ok_after(t_last_right, now)) {
        clearButtonMessage();
        xil_printf("[BTN] RIGHT  pressed\r\n");
        g_btn_msg_ticks = 1;
        t_last_right = now;
        QActive_post((QActive *)&AO_Lab2A, BTN_RIGHT);
    }

    if ((rising & BTN_DOWN_MASK) && btn_ok_after(t_last_down, now)) {
        clearButtonMessage();
        xil_printf("[BTN] DOWN   pressed\r\n");
        g_btn_msg_ticks = 1;
        t_last_down = now;
        QActive_post((QActive *)&AO_Lab2A, BTN_DOWN);
    }

    if ((rising & BTN_CENTER_MASK) && btn_ok_after(t_last_ctr, now)) {
        clearButtonMessage();
        xil_printf("[BTN] CENTER pressed\r\n");
        g_btn_msg_ticks = 1;
        t_last_ctr = now;

        /* Post to Lab2A active object */
        QActive_post((QActive *)&AO_Lab2A, BTN_CENTER);
    }
}

static void switches_poll(void) {
    u32 v = XGpio_DiscreteRead(&gpio_sw, SW_GPIO_CHANNEL) & SW0_MASK;
    u32 rising = v & ~g_sw_prev;
    u32 falling = (~v) & g_sw_prev;
    g_sw_prev = v;

    if (rising & SW0_MASK) {
        xil_printf("[SW] SW0 ON\r\n");
        QActive_post((QActive *)&AO_Lab2A, SW0_ON);
    } else if (falling & SW0_MASK) {
        xil_printf("[SW] SW0 OFF\r\n");
        QActive_post((QActive *)&AO_Lab2A, SW0_OFF);
    }
}





///******************************************************************************
//*
//* This is the interrupt handler routine for the GPIO for this example.
//*
//******************************************************************************/
//void GpioHandler(void *CallbackRef) {
//	// Increment A counter
//}
//
//void TwistHandler(void *CallbackRef) {
//	//XGpio_DiscreteRead( &twist_Gpio, 1);
//
//}
//
//void debounceTwistInterrupt(){
//	// Read both lines here? What is twist[0] and twist[1]?
//	// How can you use reading from the two GPIO twist input pins to figure out which way the twist is going?
//}
//
//void debounceInterrupt() {
//	QActive_postISR((QActive *)&AO_Lab2A, ENCODER_CLICK);
//	// XGpio_InterruptClear(&sw_Gpio, GPIO_CHANNEL1); // (Example, need to fill in your own parameters
//}
