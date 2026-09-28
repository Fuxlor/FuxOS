typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned long long uint64_t;

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
    puts("kmain est a l'adresse ");
    print_hex((uint64_t)kmain);
    puts("\n");
    puts("Le .bss fait ");
    print_dec((uint64_t)(__bss_end - __bss_start));
    puts(" octets\n\n");

    /* 3. Test du défilement : quand l'écran est plein, tout remonte */
    for (int i = 1; i <= 20; i++) {
        puts("Ligne ");
        print_dec(i);
        puts("\n");
    }

    for (;;)
        __asm__ volatile ("hlt");
}