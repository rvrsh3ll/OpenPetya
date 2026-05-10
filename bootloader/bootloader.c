// bootloader.c
// Standard libraries are not available, so they have to be purely implemented

#include "bootloader.h"

#define VGA_BASE ((volatile uint16_t *)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25

#define COLOR_WHITE_ON_BLACK    0x07
#define COLOR_GREEN_ON_BLACK    0x0A
#define COLOR_CYAN_ON_BLACK     0x0B
#define COLOR_YELLOW_ON_BLACK   0x0E
#define COLOR_WHITE_ON_BLUE     0x1F

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile (
        "outb %0, %1" :: "a"(val), "Nd"(port)
    );
}

// Global variables
static int cursor_row = 0;
static int cursor_col = 0;
static uint8_t current_color = COLOR_WHITE_ON_BLACK;

static void update_cursor(void)
{
    uint16_t pos = cursor_row * VGA_COLS + cursor_col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void scroll(void)
{
    volatile uint16_t *vga = VGA_BASE;

    for (int row = 0; row < VGA_ROWS - 1; row++)
        for (int col = 0; col < VGA_COLS; col++)
            vga[row * VGA_COLS + col] = vga[(row + 1) * VGA_COLS + col];

    uint16_t blank = ' ' | ((uint16_t)current_color << 8);
    for (int col = 0; col < VGA_COLS; col++)
        vga[(VGA_ROWS - 1) * VGA_COLS + col] = blank;
    cursor_row = VGA_ROWS - 1;
}

static void itoa(uint32_t n, char *buf, int base)
{
    const char digits[] = "0123456789ABCDEF";
    char tmp[32];
    int i = 0;
    if (n == 0)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    while (n > 0)
    {
        tmp[i++] = digits[n % base];
        n /= base;
    }

    int j = 0;
    while (i > 0)
        buf[j++] = tmp[--i];

    buf[j] = '\0';
}

static void draw_line(void)
{
    for (int i = 0; i < VGA_COLS; i++)
        vga_putchar('-');
}

static void get_cpu_vendor(char *out) {
    uint32_t ebx, ecx, edx;

    __asm__ volatile (
        "cpuid"
        : "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );

    uint32_t *p = (uint32_t *)out;
    p[0] = ebx;
    p[1] = edx;
    p[2] = ecx;
    
    out[12] = '\0';
}

void vga_clear(void)
{
    volatile uint16_t *vga = VGA_BASE;
    uint16_t blank = ' ' | (COLOR_WHITE_ON_BLACK << 8);
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        vga[i] = blank;
    cursor_row = cursor_col = 0;
    update_cursor();
}

void vga_putchar(char c)
{
    volatile uint16_t *vga = VGA_BASE;

    if (c == '\n')
    {
        cursor_col = 0;
        cursor_row++;
    }
    else if (c == '\r')
    {
        cursor_col = 0;
    }
    else if (c == '\t')
    {
        cursor_col = (cursor_col + 8) & ~7;
    }
    else
    {
        vga[cursor_row * VGA_COLS + cursor_col] = (uint8_t)c | ((uint16_t)current_color << 8);
        cursor_col++;
    }

    if (cursor_col >= VGA_COLS) {
        cursor_col = 0;
        cursor_row++;
    }

    if (cursor_row >= VGA_ROWS)
        scroll();

    update_cursor();
}

void vga_puts(const char *s)
{
    while (*s)
        vga_putchar(*s++);
}

void vga_set_color(uint8_t color)
{
    current_color = color;
}

void vga_put_dec(uint32_t n)
{
    char buf[16];
    itoa(n, buf, 10);
    vga_puts(buf);
}

void vga_put_hex(uint32_t n)
{
    char buf[16];
    vga_puts("0x");
    itoa(n, buf, 16);
    vga_puts(buf);
}

void bootloader_main(uint32_t boot_drive)
{
    vga_clear();

    vga_set_color(COLOR_WHITE_ON_BLUE);
    vga_puts("OpenBootloader v0.1  -  Stage 2 Bootloader (32-bit Protected Mode)");
    vga_set_color(COLOR_WHITE_ON_BLACK);
    vga_putchar('\n');

    vga_putchar('\n');
    vga_set_color(COLOR_CYAN_ON_BLACK);
    vga_puts("[CPU]\n");
    vga_set_color(COLOR_WHITE_ON_BLACK);

    char vendor[13];
    get_cpu_vendor(vendor);
    vga_puts("Vendor: "); vga_puts(vendor); vga_putchar('\n');

    uint32_t max_leaf;
    __asm__ volatile ("cpuid" : "=a"(max_leaf) : "a"(0) : "ebx", "ecx", "edx");
    vga_puts("Max CPUID leaf: "); vga_put_hex(max_leaf); vga_putchar('\n');

    vga_putchar('\n');
    vga_set_color(COLOR_CYAN_ON_BLACK);
    vga_puts("[Boot]\n");
    vga_set_color(COLOR_WHITE_ON_BLACK);
    vga_puts("Boot drive: ");
    vga_put_hex(boot_drive);
    vga_puts(boot_drive >= 0x80 ? "(Hard Disk)\n" : "(Floppy)\n");

    vga_putchar('\n');
    vga_set_color(COLOR_CYAN_ON_BLACK);
    vga_puts("[Memory Layout]\n");
    vga_set_color(COLOR_WHITE_ON_BLACK);
    vga_puts("0x00007C00  MBR (512 bytes)\n");
    vga_puts("0x00008000  Stage2 Bootloader  <-- I am here!\n");
    vga_puts("0x00090000  Stack top\n");
    vga_puts("0x00100000  Kernel load target\n");

    vga_putchar('\n');
    vga_set_color(COLOR_CYAN_ON_BLACK);
    vga_puts("[Kernel Loading]\n");
    vga_set_color(COLOR_WHITE_ON_BLACK);
    vga_puts("Loading kernel...");

    vga_puts("[");
    vga_set_color(COLOR_GREEN_ON_BLACK);
    for (int i = 0; i < 32; i++) {
        for (volatile int j = 0; j < 200000; j++);
        vga_putchar('#');
    }
    vga_set_color(COLOR_WHITE_ON_BLACK);
    vga_puts("]\n");

    vga_set_color(COLOR_GREEN_ON_BLACK);
    vga_puts("Done!\n");
    vga_set_color(COLOR_WHITE_ON_BLACK);

    vga_putchar('\n');
    draw_line();
    vga_set_color(COLOR_YELLOW_ON_BLACK);
    vga_puts("\nKernel would start here. System halted.\n");
    vga_set_color(COLOR_WHITE_ON_BLACK);

    __asm__ volatile ("cli\n.Lhang: hlt\njmp .Lhang\n");
    __builtin_unreachable();
}