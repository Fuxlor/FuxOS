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

// ACCES PORTS I/O
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ==================================================
   PIC
   ================================================== */

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

static void pic_remap(void)
{
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    outb(PIC1_CMD, 0x11);   /* ICW1 : démarre la séquence d'init */
    outb(PIC2_CMD, 0x11);

    outb(PIC1_DATA, 0x20);  /* ICW2 : IRQ 0-7  -> vecteurs 32-39 */
    outb(PIC2_DATA, 0x28);  /* ICW2 : IRQ 8-15 -> vecteurs 40-47 */

    outb(PIC1_DATA, 0x04);  /* ICW3 : dit au maître qu'un esclave est sur IRQ2 */
    outb(PIC2_DATA, 0x02);  /* ICW3 : dit à l'esclave son numéro d'IRQ (2) */

    outb(PIC1_DATA, 0x01);  /* ICW4 : mode 8086 */
    outb(PIC2_DATA, 0x01);

    outb(PIC1_DATA, mask1); /* restaure les masques d'origine */
    outb(PIC2_DATA, mask2);
}

/* ==================================================
   PIT (Timer)
   ================================================== */

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQ     1193182

static volatile uint64_t timer_ticks = 0;

static void pit_init(uint32_t freq_hz)
{
    uint16_t divisor = PIT_FREQ / freq_hz;

    outb(PIT_COMMAND, 0x36);              /* canal 0, lobit puis hibit, mode 3 (carré) */
    outb(PIT_CHANNEL0, divisor & 0xFF);        /* octet bas */
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF); /* octet haut */
}

/* ==================================================
   Clavier
   ================================================== */

#define KBD_DATA 0x60

static volatile int shift_pressed = 0;

/* Scancode Set 1, disposition US (indices 0x00 à 0x39 utiles) */
static const char scancode_ascii[] = {
    0,    0,   '1', '2', '3', '4', '5', '6', '7', '8',   /* 0x00-0x09 */
    '9',  '0', '-', '=', '\b','\t','q', 'w', 'e', 'r',   /* 0x0A-0x13 */
    't',  'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,     /* 0x14-0x1D (0x1D = Ctrl) */
    'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',    /* 0x1E-0x27 */
    '\'', '`', 0,   '\\','z', 'x', 'c', 'v', 'b', 'n',    /* 0x28-0x31 (0x2A = Shift) */
    'm',  ',', '.', '/', 0,   '*', 0,   ' '               /* 0x32-0x39 */
};

static const char scancode_ascii_shift[] = {
    0,    0,   '!', '@', '#', '$', '%', '^', '&', '*',
    '(',  ')', '_', '+', '\b','\t','Q', 'W', 'E', 'R',
    'T',  'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A',  'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"',  '~', 0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N',
    'M',  '<', '>', '?', 0,   '*', 0,   ' '
};

#define SC_LSHIFT      0x2A
#define SC_RSHIFT      0x36
#define SC_LSHIFT_UP   0xAA
#define SC_RSHIFT_UP   0xB6

static void keyboard_handler(void)
{
    uint8_t sc = inb(KBD_DATA);

    if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
        shift_pressed = 1;
        return;
    }
    if (sc == SC_LSHIFT_UP || sc == SC_RSHIFT_UP) {
        shift_pressed = 0;
        return;
    }

    if (sc & 0x80)                          /* relâchement d'une autre touche : ignoré */
        return;

    if (sc >= sizeof(scancode_ascii))       /* touche hors de notre table */
        return;

    char c = shift_pressed ? scancode_ascii_shift[sc] : scancode_ascii[sc];
    if (c != 0)
        putc(c);
}

/* ==================================================
   Handlers interruptions
   ================================================== */

static void irq_handler(int vector)
{
    if (vector == 32)          /* IRQ0 = timer */
        timer_ticks++;
    else if (vector == 33)         /* IRQ1 = clavier */
        keyboard_handler();

    if (vector >= 40)
        outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}

#define IRQ_HANDLER(n)                                                \
    __attribute__((interrupt))                                        \
    static void irq##n(struct interrupt_frame *frame)                 \
    {                                                                  \
        (void)frame;                                                   \
        irq_handler(32 + n);                                           \
    }

IRQ_HANDLER(0)  IRQ_HANDLER(1)  IRQ_HANDLER(2)  IRQ_HANDLER(3)
IRQ_HANDLER(4)  IRQ_HANDLER(5)  IRQ_HANDLER(6)  IRQ_HANDLER(7)
IRQ_HANDLER(8)  IRQ_HANDLER(9)  IRQ_HANDLER(10) IRQ_HANDLER(11)
IRQ_HANDLER(12) IRQ_HANDLER(13) IRQ_HANDLER(14) IRQ_HANDLER(15)

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

    void *irq_handlers[16] = {
        irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7,
        irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15
    };

    for (int i = 0; i < 16; i++)
        idt_set_entry(32 + i, irq_handlers[i]);

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

    color = 0x0F;
    cursor_row = 3;

    puts("Kernel C OK!\n");

    idt_init();
    kprintf("IDT chargee (%d entrees)\n", (int64_t)32);

    pic_remap();
    pit_init(100);              /* 100 ticks par seconde */
    __asm__ volatile ("sti");
    kprintf("Interruptions activees\n");

    kprintf("Tape au clavier : ");

    while (1) {
        __asm__ volatile ("hlt");
    }
}