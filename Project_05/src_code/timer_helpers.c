#include "timer_helpers.h"
#include "interrupt_helpers.h"

XTmrCtr TimerPWM;       // AXI Timer 0
XTmrCtr TimerIrpt;      // AXI Timer 1

volatile u16 resample;
volatile u32 gSystemTicks = 0;

void timer_pwm_init(void) {
    XTmrCtr_Initialize(&TimerPWM, TIMER_0_DEVICE_ID);
    XTmrCtr_SetOptions(&TimerPWM, 0, XTC_AUTO_RELOAD_OPTION | XTC_EXT_COMPARE_OPTION);
    XTmrCtr_PwmDisable(&TimerPWM);
    XTmrCtr_PwmConfigure(&TimerPWM, PWM_PERIOD_NS, 100);     // Default to 0 PWM to start
    XTmrCtr_PwmEnable(&TimerPWM);
}

void timer_interrupt_init(void) {
    XTmrCtr_Initialize(&TimerIrpt, TIMER_1_DEVICE_ID);
    XTmrCtr_SetResetValue(&TimerIrpt, 0, SAMPLE_PERIOD);
    XTmrCtr_SetOptions(&TimerIrpt, 0,
                       XTC_DOWN_COUNT_OPTION |
                       XTC_AUTO_RELOAD_OPTION |
                       XTC_INT_MODE_OPTION);
    // Mak sure INTC and Timer started
}

//TODO Timer 0 and 1 init
void timer_init(void) {
    timer_pwm_init();
    timer_interrupt_init();     // Make sure interrupts are enabled somewhere else
}


//TODO Timer ISR
void timer_handler(void) {
    u32 csr = XTmrCtr_ReadReg(TimerIrpt.BaseAddress, 0, XTC_TCSR_OFFSET);

    resample++;
    gSystemTicks++;

    XTmrCtr_WriteReg(TimerIrpt.BaseAddress, 0, XTC_TCSR_OFFSET,
                     csr | XTC_CSR_INT_OCCURED_MASK);
}

void pwm_set_duty(u32 duty_ticks) {
    if(duty_ticks > PWM_PERIOD) duty_ticks = PWM_PERIOD;

    XTmrCtr_SetResetValue(&TimerPWM, 1, duty_ticks);
    // XTmrCtr_PwmConfigure(&TimerPWM, PWM_PERIOD, duty_ticks);
}
