#include "interrupt_helpers.h"
#include "timer_helpers.h"
#include "gpio.h"

XIntc   IntcInst;

void interrupt_timer_init() {
    // INTC
    XIntc_Connect(&IntcInst, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_1_INTERRUPT_INTR, 
        (XInterruptHandler)timer_handler, &TimerIrpt);
    
    XIntc_Enable(&IntcInst, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_1_INTERRUPT_INTR);
}

static void interrupt_gpio_init(void) {
#if defined(XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_KEYS_IP2INTC_IRPT_INTR)
    XIntc_Connect(&IntcInst, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_KEYS_IP2INTC_IRPT_INTR,
                  (XInterruptHandler)gpio_button_isr, &KeyGpio);
    XIntc_Enable(&IntcInst, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_KEYS_IP2INTC_IRPT_INTR);
#else
    // Keys IP in this design does not expose an interrupt line; polling is used instead.
#endif
}

void interrupts_init() {
    XIntc_Initialize(&IntcInst, XPAR_MICROBLAZE_0_AXI_INTC_DEVICE_ID);

    interrupt_timer_init();
    interrupt_gpio_init();

    XIntc_Start(&IntcInst, XIN_REAL_MODE);
    microblaze_register_handler((XInterruptHandler)XIntc_DeviceInterruptHandler,
                                (void *)XPAR_MICROBLAZE_0_AXI_INTC_DEVICE_ID);
    microblaze_enable_interrupts();
}
