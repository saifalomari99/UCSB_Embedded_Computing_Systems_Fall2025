#ifndef GPIO_H
#define GPIO_H

#include "xgpio.h"
#include "xparameters.h"
#include "notes.h"

extern XGpio KeyGpio;

void gpio_init(void);
void gpio_button_isr(void *callback);

#endif
