#ifndef INC_SYSTEM_H
#define INC_SYSTEM_H

#include "common-defines.h"
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/vector.h>
#include <libopencm3/stm32/rcc.h>

#define CPU_FREQ (84000000)
#define SYSTICK_FREQ (1000)

void system_setup(void);
void systick_start(void); 
uint64_t system_get_ticks(void); 
void system_delay(uint64_t ms); 

#endif