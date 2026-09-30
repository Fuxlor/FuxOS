#include <stdarg.h>

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned long long uint64_t;
typedef long long          int64_t;
typedef unsigned int       uint32_t;

/* Fournis par le linker script */
extern char __bss_start[];
extern char __bss_end[];

#define VGA_ADDR 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

static volatile uint16_t *const vga = (volatile uint16_t *)VGA_ADDR;

/* Ces variables sont dans le .bss : elles valent 0 SEULEMENT
   après le nettoyage fait au début de kmain */
static int     cursor_row;
static int     cursor_col;
static uint8_t color;


/* ==================================================
   Console
   ================================================== */

static void console_clear(void)
{
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        vga[i] = (color << 8) | ' ';
    cursor_row = 0;
    cursor_col = 0;
}

static void console_scroll(void)
{
    /* Remonter chaque ligne d'un cran */
    for (int i = 0; i < VGA_COLS * (VGA_ROWS - 1); i++)
        vga[i] = vga[i + VGA_COLS];

    /* Vider la dernière ligne */
    for (int i = 0; i < VGA_COLS; i++)
        vga[VGA_COLS * (VGA_ROWS - 1) + i] = (color << 8) | ' ';

    cursor_row = VGA_ROWS - 1;
}

static void putc(char c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else {
        vga[cursor_row * VGA_COLS + cursor_col] = (color << 8) | (uint8_t)c;
        cursor_col++;
        if (cursor_col >= VGA_COLS) {       /* fin de ligne : on passe à la suivante */
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= VGA_ROWS)             /* écran plein : on fait défiler */
        console_scroll();
}

static void puts(const char *s)
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
        buf[i++] = '0' + (n % 10);          /* chiffres dans l'ordre inverse */
        n /= 10;
    }
    while (i > 0)
        putc(buf[--i]);                     /* on les affiche à l'endroit */
}

static void print_hex(uint64_t n)
{
    const char *digits = "0123456789ABCDEF";

    puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        putc(digits[(n >> shift) & 0xF]);
}

static void kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            putc(*fmt++);
            continue;
        }

        fmt++;                              /* on saute le '%' */

        switch (*fmt) {
            case 'd': {
                int64_t n = va_arg(args, int64_t);
                if (n < 0) {
                    putc('-');
                    n = -n;
                }
                print_dec((uint64_t)n);
                break;
            }
            case 'x':
                print_hex(va_arg(args, uint64_t));
                break;
            case 's':
                puts(va_arg(args, const char *));
                break;
            case 'c':
                putc((char)va_arg(args, int));
                break;
            case '%':
                putc('%');
                break;
            default:
                putc('%');
                putc(*fmt);
                break;
        }
        fmt++;
    }

    va_end(args);
}

/* ==================================================
   IDT
   ================================================== */

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

/* Type requis par GCC pour les fonctions __attribute__((interrupt)).
   Une déclaration incomplète suffit : on ne s'en sert que via un pointeur. */
struct interrupt_frame;

static void idt_set_entry(int n, void *handler)
{
    uint64_t addr = (uint64_t)handler;

    idt[n].offset_low  = addr & 0xFFFF;
    idt[n].selector    = 0x18;          /* segment code 64-bit de ta GDT */
    idt[n].ist         = 0;
    idt[n].type_attr   = 0x8E;          /* present, ring 0, interrupt gate */
    idt[n].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].zero        = 0;
}

static void idt_load(void)
{
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idtp));
}


/* ==================================================
   Handlers d'exceptions
   ================================================== */

static void exception_handler(int vector, uint64_t error_code)
{
    kprintf("\n=== EXCEPTION %d ===\n", (int64_t)vector);
    if (error_code)
        kprintf("Code erreur : %x\n", error_code);
    kprintf("Systeme arrete.\n");

    for (;;)
        __asm__ volatile ("cli; hlt");
}

/* Génère une fonction isrN, appelable directement par le CPU,
   qui retombe sur exception_handler avec le bon numéro */
#define ISR_NOERR(n)                                                  \
    __attribute__((interrupt))                                        \
    static void isr##n(struct interrupt_frame *frame)                 \
    {                                                                  \
        (void)frame;                                                   \
        exception_handler(n, 0);                                       \
    }

#define ISR_ERR(n)                                                     \
    __attribute__((interrupt))                                         \
    static void isr##n(struct interrupt_frame *frame, uint64_t err)    \
    {                                                                   \
        (void)frame;                                                    \
        exception_handler(n, err);                                      \
    }

/* Vecteurs 0 à 31 = exceptions CPU standard.
   Certaines poussent un code d'erreur sur la pile, d'autres non
   (règle fixée par Intel, pas un choix arbitraire). */
ISR_NOERR(0)   ISR_NOERR(1)   ISR_NOERR(2)   ISR_NOERR(3)
ISR_NOERR(4)   ISR_NOERR(5)   ISR_NOERR(6)   ISR_NOERR(7)
ISR_ERR(8)     ISR_NOERR(9)   ISR_ERR(10)    ISR_ERR(11)
ISR_ERR(12)    ISR_ERR(13)    ISR_ERR(14)    ISR_NOERR(15)
ISR_NOERR(16)  ISR_ERR(17)    ISR_NOERR(18)  ISR_NOERR(19)
ISR_NOERR(20)  ISR_NOERR(21)  ISR_NOERR(22)  ISR_NOERR(23)
ISR_NOERR(24)  ISR_NOERR(25)  ISR_NOERR(26)  ISR_NOERR(27)
ISR_NOERR(28)  ISR_NOERR(29)  ISR_ERR(30)    ISR_NOERR(31)

static void idt_init(void)
{
    void *handlers[32] = {
        isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
        isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };

    for (int i = 0; i < 32; i++)
        idt_set_entry(i, handlers[i]);

    idt_load();
}


/* ==================================================
   Point d'entrée : doit être au tout début du binaire
   ================================================== */

__attribute__((section(".text.kmain")))
void kmain(void)
{
    /* 1. Mettre le .bss à zéro (volatile : empêche le compilateur
          de transformer la boucle en appel à memset, qui n'existe pas ici) */
    for (volatile char *p = __bss_start; p < __bss_end; p++)
        *p = 0;

    /* 2. Initialiser la console. On garde les 3 lignes du bootloader
          en haut de l'écran et on écrit en dessous. */
    color = 0x0F;
    cursor_row = 3;

    puts("Kernel C OK!\n");

    idt_init();
    kprintf("IDT chargee (%d entrees)\n", (int64_t)32);

    /* Test : provoquer volontairement une division par zero */
    volatile int a = 10, b = 0;
    kprintf("Test : %d\n", (int64_t)(a / b));

    kprintf("Cette ligne ne devrait jamais s'afficher\n");

    for (;;)
        __asm__ volatile ("hlt");
}