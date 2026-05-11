// petya.c

#include "petya.h"

void print_petya_art(void)
{
    vga_clear();
    vga_puts(RANSOM_MSG);    
    vga_putchar('\n');
}