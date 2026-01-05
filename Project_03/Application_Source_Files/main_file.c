
// ------------------ header files
#include "xparameters.h"
#include "xgpio.h"
#include "xtmrctr.h"
#include "xintc.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "sevenSeg_new.h"

// ------------------- I/O Devices from xparameters.h
// ---- RGB LEDs
#define RGB_GPIO_DEVICE_ID   XPAR_GPIO_4_DEVICE_ID
#define RGB_GPIO_CHANNEL     1
// ---- Timer
#define TIMER_DEV_ID   XPAR_TMRCTR_0_DEVICE_ID
#define TIMER_CH       0
#define TMR_CLK_HZ     XPAR_TMRCTR_0_CLOCK_FREQ_HZ   // e.g. 100_000_000
#define TICKS_PER_US   (TMR_CLK_HZ / 1000000U)       // e.g. 100 at 100 MHz
// ---- LED
#define LED_GPIO_DEVICE_ID   XPAR_GPIO_2_DEVICE_ID
#define LED_GPIO_CHANNEL     1
// --- Encoder GPIO (canonical for ENCODER block)
#define ENC_GPIO_DEVICE_ID   XPAR_GPIO_0_DEVICE_ID
#define ENC_GPIO_CHANNEL     1
// --- Interrupt controller
#define INTC_DEVICE_ID       XPAR_INTC_0_DEVICE_ID
#define ENC_IRQ_VEC_ID       XPAR_INTC_0_GPIO_0_VEC_ID


// ------------------- define parameters
#define ACTIVE_LOW  0                                // Active-low LEDs (0 = ON, 1 = OFF)
#define HALF_SECOND 500000
#define ONE_SECOND  1000000
// --- JA[2:0] bit mapping: A=JA[1], B=JA[0], Push=JA[2]
#define AB_A_BIT  1u
#define AB_B_BIT  0u
#define PB_BIT    2u
#define AB_A_MASK (1u << AB_A_BIT)
#define AB_B_MASK (1u << AB_B_BIT)
#define PB_MASK   (1u << PB_BIT)
#define DEBOUNCE_US     20000u                         // 20 ms
#define WAIT_TICKS(us)  ((uint32_t)((us) * TICKS_PER_US))

// ------------------- object instances:
static XGpio rgb;
XTmrCtr sys_timer;
static XGpio leds;
static XGpio gpio_enc;
static XIntc intc;
static volatile int enc_cw_flag = 0;                  // Flags set by ISR
static volatile int enc_ccw_flag = 0;
static volatile int led_pos = 0;                   // Current LED cursor (0..15)
typedef enum { ST_11=0, ST_01=1, ST_00=2, ST_10=3 } enc_state_t;
static volatile enc_state_t enc_state = ST_11;
// ---- for the push button:
static volatile int  display_enabled   = 1;            // 1 = LEDs shown, 0 = off
static int           last_pb           = 0;            // last sampled push level
static int           push_pending      = 0;            // 1 while debouncing
static uint32_t      push_start_tick   = 0;            // tick when press detected
// -- extra
int Show_enable[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
int Show_disable[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };


// ------------------- functions
void rgb_set_color(int red, int green, int blue);
static void timer_init_free_running(void);
static void sleep_us(uint32_t us);
void led_set_position(int position);
static inline uint32_t enc_read_ab(void);
static inline int enc_read_pb(void);
static void enc_isr(void *cb);
static int encoder_init(void);
void init_everything();


// ========================================= Main =============================================
int main(void)
{
	init_everything();

	// --------------- while grand loop
	while (1)
	{
	    // ----------------- Push debounce + toggle -----------------
	    int pb_now = enc_read_pb();                 // 1 = pressed (active-high)
	    if (pb_now && !last_pb && !push_pending) {  // rising edge
	        push_start_tick = XTmrCtr_GetValue(&sys_timer, TIMER_CH); // down-counter
	        push_pending    = 1;
	    }
	    last_pb = pb_now;

	    if (push_pending) {
	        uint32_t now     = XTmrCtr_GetValue(&sys_timer, TIMER_CH);
	        uint32_t elapsed = (uint32_t)(push_start_tick - now);     // wrap-safe for down-counter
	        if (elapsed >= WAIT_TICKS(DEBOUNCE_US)) {
	            push_pending = 0;

	            // Toggle display
	            display_enabled ^= 1;
	            if (display_enabled) {
	                led_set_position(led_pos);                         // restore last position
	                rgb_set_color(0, 1, 0);                           // (optional) green blink
	            } else {
	                XGpio_DiscreteWrite(&leds, LED_GPIO_CHANNEL, 0x0);// all LEDs off
	                rgb_set_color(0, 0, 0);                           // (optional) RGB off
	            }
	        }
	    }

	    // ----------------- Rotation handling (gated) -----------------
	    if (display_enabled) {
	        if (enc_cw_flag) {
	            enc_cw_flag = 0;
	            led_pos = (led_pos + 1) & 15;
	            led_set_position(led_pos);
	        }
	        if (enc_ccw_flag) {
	            enc_ccw_flag = 0;
	            led_pos = (led_pos - 1) & 15;
	            led_set_position(led_pos);
	        }
	    } else {
	        // Ignore any rotations while off
	        enc_cw_flag  = 0;
	        enc_ccw_flag = 0;
	    }

	    // (optional) tiny relax
	    //sleep_us(1000);
	    // multiplex all 8 digits quickly
	    // Draw them (basic multiplex refresh)
	    for (int pos = 0; pos < 8; pos++) {
	    	if (display_enabled)
	    		sevenseg_draw_digit(pos, Show_enable[pos]);
	    	else
	    		sevenseg_draw_digit(pos, Show_disable[pos]);
	        sleep_us(1000); // small scan delay per digit
	    }
	}// end while loop
}
// ==============================================================================================

void init_everything()
{
	timer_init_free_running();
    XGpio_Initialize(&rgb, RGB_GPIO_DEVICE_ID);
    XGpio_Initialize(&leds, LED_GPIO_DEVICE_ID);
    XGpio_SetDataDirection(&leds, LED_GPIO_CHANNEL, 0x0000);              // all outputs
    // ===== encoder interrupts =====
    if (encoder_init() != XST_SUCCESS) {
        xil_printf("Encoder init failed\r\n");
        while (1) {}
    }
    xil_printf("Encoder ready. CW->right, CCW->left.\r\n");

    led_set_position(led_pos);
}


// ===================== Function =========================
// red, green, blue = 1 or 0 to turn that color ON/OFF
// Example: rgb_set_color(1, 0, 1) -> Red + Blue (magenta)
// ========================================================
void rgb_set_color(int red, int green, int blue)
{
    // Construct the 3-bit pattern: bit2=R, bit1=G, bit0=B
    unsigned value = (red ? (1u << 2) : 0) |
                     (green ? (1u << 1) : 0) |
                     (blue ? (1u << 0) : 0);

    // Adjust for active-low
    if (ACTIVE_LOW)
        value = (~value) & 0b111;

    // Write to GPIO
    XGpio_DiscreteWrite(&rgb, RGB_GPIO_CHANNEL, value);



}


// Call once at startup
static void timer_init_free_running(void)
{
    XTmrCtr_Initialize(&sys_timer, TIMER_DEV_ID);

    // Free-run, auto-reload, down-count
    XTmrCtr_SetOptions(&sys_timer, TIMER_CH,
        XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);

    // Reload value = max so it free-runs (wraps) for a long time
    XTmrCtr_SetResetValue(&sys_timer, TIMER_CH, 0xFFFFFFFFu);

    XTmrCtr_Start(&sys_timer, TIMER_CH);
}
// -------------------------------------------------------------------
// Busy-wait delay using AXI timer
// Waits for 'us' microseconds based on the hardware timer
// -------------------------------------------------------------------
// Busy wait for 'us' microseconds (wrap-safe for down-counter)
static void sleep_us(uint32_t us)
{
    const uint32_t wait_ticks = (uint32_t)((uint64_t)us * (uint64_t)TICKS_PER_US);
    const uint32_t start = XTmrCtr_GetValue(&sys_timer, TIMER_CH);

    // Because the timer counts DOWN, elapsed = start - current (mod 2^32)
    while ((uint32_t)(start - XTmrCtr_GetValue(&sys_timer, TIMER_CH)) < wait_ticks) {
        // spin
    }
}



// ===============================================================
// Turn ON a single LED by position (0–15)
// Example: led_set_position(0) -- LED0 ON
//          led_set_position(15) -- LED15 ON
// ===============================================================
void led_set_position(int position)
{
    // Clamp position to valid range (0–15)
    if (position < 0)  position = 0;
    if (position > 15) position = 15;

    // Construct a bit mask for that LED
    uint16_t value = (1u << position);

    // Write to GPIO (active-high LEDs on Nexys)
    XGpio_DiscreteWrite(&leds, LED_GPIO_CHANNEL, value);
}


// Read A/B as a 2-bit value {B,A} = (b<<1)|a  (returns 0..3)
static inline uint32_t enc_read_ab(void)
{
    uint32_t v = XGpio_DiscreteRead(&gpio_enc, ENC_GPIO_CHANNEL);
    uint32_t a = (v & AB_A_MASK) ? 1u : 0u;
    uint32_t b = (v & AB_B_MASK) ? 1u : 0u;
    return ((b << 1) | a);                                  // 0..3 representing {B,A}
}
// Read push button: returns 1 if pressed, 0 if not
static inline int enc_read_pb(void)
{
    uint32_t v = XGpio_DiscreteRead(&gpio_enc, ENC_GPIO_CHANNEL);
    return (v & PB_MASK) ? 1 : 0;   // pressed = high per your wiring
}


static void enc_isr(void *cb)
{
    // Latch & clear GPIO interrupt early
    uint32_t pending = XGpio_InterruptGetStatus(&gpio_enc);
    XGpio_InterruptClear(&gpio_enc, pending);
    (void)cb;

    uint32_t ab = enc_read_ab();

    switch (enc_state) {
    case ST_11:
        if (ab == 0b01) enc_state = ST_01;      // possible CW path
        else if (ab == 0b10) enc_state = ST_10; // possible CCW path
        break;
    case ST_01:
        if      (ab == 0b00) enc_state = ST_00;
        else if (ab == 0b11) { enc_ccw_flag = 1; enc_state = ST_11; } // …01->11 = CCW
        break;
    case ST_00:
        if      (ab == 0b01) enc_state = ST_01;
        else if (ab == 0b10) enc_state = ST_10;
        break;
    case ST_10:
        if      (ab == 0b00) enc_state = ST_00;
        else if (ab == 0b11) { enc_cw_flag = 1; enc_state = ST_11; }  // …10->11 = CW
        break;
    }
}


static int encoder_init(void)
{
    int st;

    // Encoder as inputs
    st = XGpio_Initialize(&gpio_enc, ENC_GPIO_DEVICE_ID);
    if (st != XST_SUCCESS) return st;
    XGpio_SetDataDirection(&gpio_enc, ENC_GPIO_CHANNEL, 0xFFFFFFFFu);

    // Interrupt controller
    st = XIntc_Initialize(&intc, INTC_DEVICE_ID);
    if (st != XST_SUCCESS) return st;

    st = XIntc_Connect(&intc, ENC_IRQ_VEC_ID, (XInterruptHandler)enc_isr, NULL);
    if (st != XST_SUCCESS) return st;

    XIntc_Start(&intc, XIN_REAL_MODE);
    XIntc_Enable(&intc, ENC_IRQ_VEC_ID);

    // Enable GPIO interrupts (per-bit ok; we’ll just enable all)
    XGpio_InterruptGlobalEnable(&gpio_enc);
    XGpio_InterruptEnable(&gpio_enc, AB_A_MASK | AB_B_MASK | PB_MASK);

    // Hook MicroBlaze
    microblaze_register_handler((XInterruptHandler)XIntc_DeviceInterruptHandler,
                                (void*)(INTC_DEVICE_ID));
    microblaze_enable_interrupts();

    return XST_SUCCESS;
}





