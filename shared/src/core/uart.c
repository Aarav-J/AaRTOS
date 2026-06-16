#include "core/uart.h"
#include "core/ring-buffer.h"
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h> 


#define BAUD_RATE (115200)
#define RING_BUFFER_SIZE 64

static ring_buffer_t rb = {0U}; 

static uint8_t data_buffer[RING_BUFFER_SIZE] = {0U}; 
// static bool data_available = false; 

// Interrupt handler for USART2, which is the USART peripheral we are using for UART communication. This function will be called when a USART2 interrupt occurs, such as when data is received on USART2. We can handle the received data in this function. 
void usart2_isr(void) { 
    const bool overrun_occured = usart_get_flag(USART2, USART_FLAG_ORE); // Check if an overrun error occurred, which means that data was received but not read before the next data was received, causing the previous data to be lost. We can check this flag to handle overrun errors if needed.
    const bool received_data = usart_get_flag(USART2, USART_FLAG_RXNE);

    if (received_data || overrun_occured) { 
        if(ring_buffer_write(&rb, (uint8_t) usart_recv(USART2))) { 
            //Handle Failure?
        } 
        
    }
}

void uart_setup(void) { 
    ring_buffer_setup(&rb, data_buffer, RING_BUFFER_SIZE); // Setup the ring buffer for storing received data. The ring buffer will be used to store incoming data from the UART in a circular buffer, allowing us to handle variable amounts of incoming data without losing any data due to buffer overflow.


    rcc_periph_clock_enable(RCC_USART2); // Enable clock for USART2 peripheral. Use number 2 because we are using PA2 and PA3
    usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE); // No flow control, as we are not using RTS and CTS pins for flow control
    usart_set_databits(USART2, 8); 
    usart_set_baudrate(USART2, BAUD_RATE);
    usart_set_parity(USART2, 0); 
    usart_set_stopbits(USART2, USART_STOPBITS_1); // 1 stop bit

    usart_set_mode(USART2, USART_MODE_TX_RX); // Enable both transmission and reception mode, so that we can send and receive data using USART2
    // Enable interrupts 
    usart_enable_rx_interrupt(USART2); // Enable receive interrupt, so that when data is received on USART2, it will generate an interrupt and jump to the USART2 ISR (Interrupt Service Routine) where we can handle the received data
    nvic_enable_irq(NVIC_USART2_IRQ); // Enable USART2 interrupt in the NVIC, so that when USART2 generates an interrupt, the CPU will jump to the USART2 ISR
    usart_enable(USART2); // Finally enable the USART2 peripheral
}
void uart_write(uint8_t* data, const uint32_t length) { 
    for (uint32_t i = 0; i < length; i++) { 
        usart_send_blocking(USART2, (uint16_t) data[i]); // Send the data using USART2 in a blocking way, which means the function will wait until all the data is sent before returning
    }
} 
void uart_write_byte(uint8_t byte) { 
    usart_send_blocking(USART2, (uint16_t) byte); // Send the data using USART2 in a blocking way, which means the function will wait until all the data is sent before returning
} 
uint32_t uart_read(uint8_t* data, const uint32_t length) { 
    if(length == 0) { 
        return 0; 
    } 
    for (uint32_t bytes_read = 0; bytes_read < length; bytes_read++) { 
        if(!ring_buffer_read(&rb, &data[bytes_read])) { 
            return bytes_read; 
        }
    }
    return length; 
} 
uint8_t uart_read_byte(void) { 
    uint8_t byte = 0; 
    (void) uart_read(&byte, 1); // Read a single byte from the ring buffer using the uart_read function, which will read data from the ring buffer and store it in the provided byte variable. We specify a length of 1 to read only one byte.
    return byte;
} 
bool uart_data_available(void) { 
    return !ring_buffer_empty(&rb); 
} 