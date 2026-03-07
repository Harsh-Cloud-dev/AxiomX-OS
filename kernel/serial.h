#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

#define COM1 0x3F8
#define COM2 0x2F8
#define COM3 0x3E8
#define COM4 0x2E8

int  serial_init(uint16_t port);
void serial_putc(char c);
void serial_puts(const char *s);
void serial_puthex(uint64_t val);
void serial_putdec(uint64_t val);
void serial_printf(const char *fmt, ...);

#endif