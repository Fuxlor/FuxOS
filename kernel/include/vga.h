#ifndef VGA_H
#define VGA_H

#include "types.h"

void console_clear(void);
void putc(char c);
void puts(const char *s);
void kprintf(const char *fmt, ...);

#endif