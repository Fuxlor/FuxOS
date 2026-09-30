#include "keyboard.h"
#include "io.h"
#include "vga.h"

#define KBD_DATA 0x60
#define KBD_STATUS 0x64

static volatile int shift_pressed = 0;

static const char scancode_ascii[] = {
    0,    0,   '1', '2', '3', '4', '5', '6', '7', '8',
    '9',  '0', '-', '=', '\b','\t','q', 'w', 'e', 'r',
    't',  'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0,   '\\','z', 'x', 'c', 'v', 'b', 'n',
    'm',  ',', '.', '/', 0,   '*', 0,   ' '
};

static const char scancode_ascii_shift[] = {
    0,    0,   '!', '@', '#', '$', '%', '^', '&', '*',
    '(',  ')', '_', '+', '\b','\t','Q', 'W', 'E', 'R',
    'T',  'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A',  'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"',  '~', 0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N',
    'M',  '<', '>', '?', 0,   '*', 0,   ' '
};

#define SC_LSHIFT     0x2A
#define SC_RSHIFT     0x36
#define SC_LSHIFT_UP  0xAA
#define SC_RSHIFT_UP  0xB6

void keyboard_flush(void)
{
    while (inb(KBD_STATUS) & 0x1)
        inb(KBD_DATA);
}

void keyboard_handle(void)
{
    uint8_t sc = inb(KBD_DATA);

    if (sc == SC_LSHIFT || sc == SC_RSHIFT) { shift_pressed = 1; return; }
    if (sc == SC_LSHIFT_UP || sc == SC_RSHIFT_UP) { shift_pressed = 0; return; }
    if (sc & 0x80) return;
    if (sc >= sizeof(scancode_ascii)) return;

    char c = shift_pressed ? scancode_ascii_shift[sc] : scancode_ascii[sc];
    if (c != 0)
        putc(c);
}