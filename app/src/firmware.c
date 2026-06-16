#include <stddef.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/scb.h>
#include "core/system.h"
#include "core/uart.h"
#include "core/print.h"
#include "core/kernel.h"

#define BOOTLOADER_SIZE (0x8000U)

#define LED_PORT (GPIOA)
#define LED_PIN  (GPIO5)
#define UART_PORT    (GPIOA)
#define UART_RX_PIN  (GPIO3)
#define UART_TX_PIN  (GPIO2)

static void vector_setup(void) {
    SCB_VTOR = BOOTLOADER_SIZE;
}

static void gpio_setup(void) {
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_mode_setup(UART_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, UART_RX_PIN | UART_TX_PIN);
    gpio_set_af(UART_PORT, GPIO_AF7, UART_RX_PIN | UART_TX_PIN);
}

static mutex_t uart_mutex = MUTEX_INIT;

static void led_task(void *arg __attribute__((unused))) {
    int count = 0;
    gpio_set(LED_PORT, LED_PIN);
    while (1) {
        mutex_lock(&uart_mutex);
        println("Led on");
        mutex_unlock(&uart_mutex);
        gpio_clear(LED_PORT, LED_PIN);
        os_delay(250);
        mutex_lock(&uart_mutex);
        println("Led off");
        mutex_unlock(&uart_mutex);
        gpio_set(LED_PORT, LED_PIN);
        os_delay(250);
        count++;
    }
}

static void print_task(void *arg __attribute__((unused))) {
    int count = 0;
    while (1) {
        mutex_lock(&uart_mutex);
        print("Hello, Count:");
        print_dec(count);
        print("\r\n");
        mutex_unlock(&uart_mutex);
        os_delay(1000);
        count++;
    }
}

int main(void) {
    vector_setup();
    system_setup();
    gpio_setup();
    uart_setup();

    os_kernel_init();

    os_task_create("LED", led_task, (void*)0);
    os_task_create("PRINT", print_task, (void*)0);

    os_kernel_start();

    return 0;
}
