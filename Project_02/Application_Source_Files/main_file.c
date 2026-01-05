
// ---- included libraries
#include <stdio.h>	       	// Used for printf()
#include <stdlib.h>		    // Used for rand()
#include "xparameters.h"	// Contains hardware addresses and bit masks
#include "xil_cache.h"		// Cache Drivers
#include "xintc.h"		    // Interrupt Drivers
#include "xtmrctr.h"		// Timer Drivers
#include "xtmrctr_l.h" 		// Low-level timer drivers
#include "xil_printf.h" 	// Used for xil_printf()
//#include "extra.h" 		    // Provides a source of bus contention
#include "xgpio.h" 		    // LED driver, used for General purpose I/i
#include "sevenSeg_new.h"
//#include <xbasic_types.h>
#include "xil_exception.h"
#include <stdbool.h>

XIntc sys_intc_1;
XTmrCtr sys_tmrctr_1;


// Globals
volatile unsigned total_ms = 0;
volatile unsigned total_sec = 0;

// ---- stopwatch state
volatile int running   = 0;   // 0 = stopped, 1 = running
volatile int direction = +1;  // +1 = up, -1 = down

// ---- buttons (polling)
#define BTN_GPIO_DEVICE_ID  XPAR_AXI_GPIO_BTN_DEVICE_ID
#define GPIO_INT_ID         XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR
#define BTN_GPIO_CHANNEL    1
// Button bit masks (from your XDC)
#define BTN_UP_MASK     (1u << 0)   // btn[0]
#define BTN_LEFT_MASK   (1u << 1)   // btn[1]
#define BTN_RIGHT_MASK  (1u << 2)   // btn[2]
#define BTN_DOWN_MASK   (1u << 3)   // btn[3]
#define BTN_CENTER_MASK (1u << 4)   // btn[4]

static XGpio gpio_buttons;



static void buttons_init(void);
void timer_handler();
void timer_init();
static inline void short_delay(volatile int n);
static inline void render_sec_ms_to_digits(unsigned ms, unsigned sec, unsigned char digs[8]);




// ============================== main ===========================================
int main(void)
{
    xil_printf("Stopwatch w/ 5 buttons: up=start, down=stop, right=count up, left=count down, center=reset\r\n");

    timer_init();
    buttons_init();

    unsigned char digs[8];
    int pos = 0;
    render_sec_ms_to_digits(total_ms, total_sec, digs);

    u32 last_btns = 0;

    while (1)
    {
        sevenseg_draw_digit(pos, digs[pos]);
        pos = (pos + 1) & 7;
        short_delay(50);

        u32 btns = XGpio_DiscreteRead(&gpio_buttons, BTN_GPIO_CHANNEL);

        // Reset (center)
        if ((btns & BTN_CENTER_MASK) && !(last_btns & BTN_CENTER_MASK)) {
            running = 0;
            direction = +1;
            total_ms = 0;
            total_sec = 0;
            xil_printf("Reset to 00:00.000\r\n");
        }

        // Start (right)
        if ((btns & BTN_RIGHT_MASK) && !(last_btns & BTN_RIGHT_MASK)) {
            running = 1;
            direction = +1;
            xil_printf("Start\r\n");
        }

        // Stop (left)
        if ((btns & BTN_LEFT_MASK) && !(last_btns & BTN_LEFT_MASK)) {
            running = 0;
            xil_printf("Stop\r\n");
        }

        // Count Up (up)
        if ((btns & BTN_UP_MASK) && !(last_btns & BTN_UP_MASK)) {
            direction = +1;
            xil_printf("Count Up\r\n");
        }

        // Count Down (down)
        if ((btns & BTN_DOWN_MASK) && !(last_btns & BTN_DOWN_MASK)) {
            direction = -1;
            xil_printf("Count Down\r\n");
        }

        last_btns = btns;

        render_sec_ms_to_digits(total_ms*10, total_sec, digs);
    }
}
// ============================================= End main ==============================================================================





// ============================= buttons =========================================
static void buttons_init(void)
{
    XGpio_Initialize(&gpio_buttons, BTN_GPIO_DEVICE_ID);
    XGpio_SetDataDirection(&gpio_buttons, BTN_GPIO_CHANNEL, 0xFFFFFFFFu); // inputs
}


// ISR (note: use XTmrCtr_ReadReg, not XTimerCtr_ReadReg)
void timer_handler(void *cb_ref)
{
    u32 csr = XTmrCtr_ReadReg(sys_tmrctr_1.BaseAddress, 0, XTC_TCSR_OFFSET);

    if (running) {
        if (direction > 0) {
            // ---- count up
            total_ms++;
            if (total_ms >= 1000) { total_ms = 0; total_sec++; }
            if (total_sec >= 10000) { total_sec = 0; }  // wrap 4 digits
        } else {
            // ---- count down (don�t go below 00:00.000)
            if (total_ms > 0 || total_sec > 0) {
                if (total_ms > 0) {
                    total_ms--;
                } else { // total_ms == 0
                    if (total_sec > 0) {
                        total_sec--;
                        total_ms = 999;
                    }
                }
            }
        }
    }

    // Ack timer interrupt
    XTmrCtr_WriteReg(sys_tmrctr_1.BaseAddress, 0, XTC_TCSR_OFFSET,
                     csr | XTC_CSR_INT_OCCURED_MASK);
}

// ============================== timer init =====================================
void timer_init(void)
{
    // INTC
    XIntc_Initialize(&sys_intc_1, XPAR_MICROBLAZE_0_AXI_INTC_DEVICE_ID);
    XIntc_Connect(&sys_intc_1,
                  XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR,
                  (XInterruptHandler)timer_handler, &sys_tmrctr_1);
    XIntc_Start(&sys_intc_1, XIN_REAL_MODE);
    XIntc_Enable(&sys_intc_1, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR);

    // Timer: 1 ms tick, auto-reload, down-count, interrupt
    XTmrCtr_Initialize(&sys_tmrctr_1, XPAR_AXI_TIMER_0_DEVICE_ID);
    XTmrCtr_SetOptions(&sys_tmrctr_1, 0,
        XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);

    const u32 clk_hz = XPAR_AXI_TIMER_0_CLOCK_FREQ_HZ;   // e.g., 100 MHz
    const u32 reload = (clk_hz / 1000u) - 1u;            // 1 ms
    XTmrCtr_SetResetValue(&sys_tmrctr_1, 0, reload);
    XTmrCtr_Start(&sys_tmrctr_1, 0);

    // CPU interrupt hookup
    microblaze_register_handler((XInterruptHandler)XIntc_DeviceInterruptHandler,
                                (void *)XPAR_MICROBLAZE_0_AXI_INTC_DEVICE_ID);
    microblaze_enable_interrupts();
}



// ---- A small helper that just wastes time to create a short delay.
static inline void short_delay(volatile int n)
{
	while (n--) { /* burn cycles */ }
}


// ---- Put time into 8 digits: [7..4]=seconds (0000..9999), [3..0]=milliseconds (000..999 + a fixed 0)
static inline void render_sec_ms_to_digits(unsigned ms, unsigned sec, unsigned char digs[8])
{
	// lower: 0..9999 ms (4 full digits)
	unsigned m = ms % 10000u;

	digs[0] = (unsigned char)(m % 10u);  m /= 10u;
	digs[1] = (unsigned char)(m % 10u);  m /= 10u;
	digs[2] = (unsigned char)(m % 10u);  m /= 10u;
	digs[3] = (unsigned char)(m % 10u);

    // upper: seconds 0..9999
    unsigned s = sec % 10000u;

    digs[4] = (unsigned char)(s % 10u);  s /= 10u;
    digs[5] = (unsigned char)(s % 10u);  s /= 10u;
    digs[6] = (unsigned char)(s % 10u);  s /= 10u;
    digs[7] = (unsigned char)(s % 10u);
}

