#include "vga.h"
#include "serial.h"
#include <stdarg.h>

#define VGA_ADDR 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

static volatile uint16_t *const vga = (volatile uint16_t *)VGA_ADDR;

static int     cursor_row;
static int     cursor_col;
static uint8_t color = 0x0F;

static void console_scroll(void)
{
    for (int i = 0; i < VGA_COLS * (VGA_ROWS - 1); i++)
        vga[i] = vga[i + VGA_COLS];

    for (int i = 0; i < VGA_COLS; i++)
        vga[VGA_COLS * (VGA_ROWS - 1) + i] = (color << 8) | ' ';

    cursor_row = VGA_ROWS - 1;
}

void console_clear(void)
{
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        vga[i] = (color << 8) | ' ';
    cursor_row = 0;
    cursor_col = 0;
}

void putc(char c)
{
    serial_putc(c);

    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else {
        vga[cursor_row * VGA_COLS + cursor_col] = (color << 8) | (uint8_t)c;
        cursor_col++;
        if (cursor_col >= VGA_COLS) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= VGA_ROWS)
        console_scroll();
}

void puts(const char *s)
{
    while (*s)
        putc(*s++);
}

static void print_dec(uint64_t n)
{
    char buf[21];
    int i = 0;

    if (n == 0) {
        putc('0');
        return;
    }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0)
        putc(buf[--i]);
}

static void print_hex(uint64_t n)
{
    const char *digits = "0123456789ABCDEF";
    puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        putc(digits[(n >> shift) & 0xF]);
}

void kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            putc(*fmt++);
            continue;
        }
        fmt++;

        switch (*fmt) {
            case 'd': {
                int64_t n = va_arg(args, int64_t);
                if (n < 0) { putc('-'); n = -n; }
                print_dec((uint64_t)n);
                break;
            }
            case 'x': print_hex(va_arg(args, uint64_t)); break;
            case 's': puts(va_arg(args, const char *)); break;
            case 'c': putc((char)va_arg(args, int)); break;
            case '%': putc('%'); break;
            default:  putc('%'); putc(*fmt); break;
        }
        fmt++;
    }

    va_end(args);
}