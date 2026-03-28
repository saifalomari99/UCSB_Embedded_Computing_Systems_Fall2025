#ifndef TIMER_HELPERS_H
#define TIMER_HELPERS_H

#include "xparameters.h"
#include "xtmrctr.h"
#include "xil_types.h"

// Timer assignments:
// - AXI Timer 0: PWM (TimerPWM)
// - AXI Timer 1: Audio sample interrupt (TimerIrpt)
// - AXI Timer 2: BSP free-running counter (configured in bsp.c)

#define TIMER_0_DEVICE_ID       XPAR_AXI_TIMER_0_DEVICE_ID
#define TIMER_1_DEVICE_ID       XPAR_AXI_TIMER_1_DEVICE_ID

// #define TIMER_1_DEVICE_ID      XPAR_AXI_TIMER_1_DEVICE_ID
// #define INTC_DEVICE_ID         XPAR_INTC_0_DEVICE_ID
// #define TIMER_1_IRPT_ID        XPAR_INTC_0_TMRCTR_1_VEC_ID

#define TIMER_CLOCK_HZ          100000000U      // 100 MHz

#define SAMPLE_PERIOD 			2000U      // timer ticks per sample
#define SAMPLE_RATE_HZ			50000U

#define PWM_PERIOD              2000U

#define PWM_PERIOD_NS           PWM_PERIOD*10

// Globals
extern XTmrCtr TimerPWM;
extern XTmrCtr TimerIrpt;

extern volatile u16 resample;
extern volatile u32 gSystemTicks;

// Function Alias
// XTmrCtr_Start(&TimerInst1, 0)

// Global Function
void timer_init(void);
void timer_handler(void);

void pwm_set_duty(u32 duty_ticks);

#endif
