#include "core/system.h"
#include <libopencm3/cm3/scb.h>

volatile uint64_t ticks = 0;

static void rcc_setup(void) {
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);
}

static void systick_setup(void) {
    systick_set_frequency(SYSTICK_FREQ, CPU_FREQ);
    systick_counter_enable();
    /* Interrupt enabled by kernel when ready */
}

void sys_tick_handler(void) {
    ticks++;

    /* Trigger PendSV to perform a context switch.
     * PendSV is lowest priority, so it runs after SysTick returns.
     * This makes the scheduler preemptive — every 1ms we switch tasks. */
    SCB_ICSR |= SCB_ICSR_PENDSVSET;
}

uint64_t system_get_ticks(void) {
    return ticks;
}

void system_setup(void) {
    rcc_setup();
    systick_setup();
}

void systick_start(void) {
    systick_interrupt_enable();
}

void system_delay(uint64_t ms) {
    uint64_t end_time = system_get_ticks() + ms;
    while (system_get_ticks() < end_time) {
        /* spin */
    }
}
