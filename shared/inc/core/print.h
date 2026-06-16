#ifndef INC_PRINT_H
#define INC_PRINT_H

#include <stdint.h>
#include <stdbool.h>

void print(const char *str);
void println(const char *str);
void print_hex(uint32_t val);
void print_dec(int32_t val);

#endif
