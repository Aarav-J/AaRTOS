#include "core/print.h"
#include "core/uart.h"

void print(const char *str) {
    while (*str) {
        uart_write_byte((uint8_t)*str);
        str++;
    }
}

void println(const char *str) {
    print(str);
    uart_write_byte('\r');
    uart_write_byte('\n');
}

void print_hex(uint32_t val) {
    char buf[11] = "0x00000000";
    for (int i = 9; i >= 2; i--) {
        uint8_t nibble = (uint8_t)(val & 0xF);
        buf[i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
        val >>= 4;
    }
    print(buf);
}

void print_dec(int32_t val) {
    char buf[12];
    int8_t i = 10;

    if (val < 0) {
        uart_write_byte('-');
        val = -val;
    }

    if (val == 0) {
        uart_write_byte('0');
        return;
    }

    buf[11] = '\0';
    while (val > 0 && i >= 0) {
        buf[i--] = (char)('0' + (val % 10));
        val /= 10;
    }
    print(&buf[i + 1]);
}
