// bootloader.h
// Header file

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include "types.h"

#define NULL ((void *)0)

void vga_clear(void);
void vga_putchar(char c);
void vga_puts(const char *s);
void vga_putchar_at(int row, int col, char c);
void vga_draw_centered_ascii(const char *art);
void vga_set_color(uint8_t color);
void vga_put_dec(uint32_t n);
void vga_put_hex(uint32_t n);

void bootloader_main(uint32_t boot_drive);

void do_chainload(void) __attribute__((noreturn));
void do_reboot(void) __attribute__((noreturn));

extern char __bss_start[];
extern char __bss_end[];

#endif
