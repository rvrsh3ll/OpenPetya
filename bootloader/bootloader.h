// bootloader.h
// Header file

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

#define NULL ((void *)0)

void vga_clear(void);
void vga_putchar(char c);
void vga_puts(const char *s);
void vga_set_color(uint8_t color);
void vga_put_dec(uint32_t n);
void vga_put_hex(uint32_t n);

void bootloader_main(uint32_t boot_drive);

#endif
