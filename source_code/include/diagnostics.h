#ifndef __DIAGNOSTICS_H
#define __DIAGNOSTICS_H

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>

#include "delay.h"
#include "leds.h"

void show_reset_cause(void);

#endif
