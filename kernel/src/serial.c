#include "serial.h"
#include "io.h"

#define COM1 0x3F8

void serial_init(void)
{
    outb(COM1 + 1, 0x00);    /* désactive les interruptions du port série */
    outb(COM1 + 3, 0x80);    /* active l'accès au diviseur de baud rate */
    outb(COM1 + 0, 0x03);    /* diviseur = 3 -> 38400 bauds (octet bas) */
    outb(COM1 + 1, 0x00);    /* diviseur (octet haut) */
    outb(COM1 + 3, 0x03);    /* 8 bits, pas de parité, 1 bit de stop */
    outb(COM1 + 2, 0xC7);    /* active et vide les FIFO, seuil 14 octets */
    outb(COM1 + 4, 0x0B);    /* active les lignes de contrôle (RTS/DSR) */
}

static int serial_transmit_empty(void)
{
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char c)
{
    while (!serial_transmit_empty())
        ;                      /* attend que le port soit prêt à envoyer */

    outb(COM1, (uint8_t)c);
}

void serial_puts(const char *s)
{
    while (*s)
        serial_putc(*s++);
}