// keyboard.h

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "bootloader.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

void keyboard_int(void);
char keyboard_getchar(void);
void keyboard_readline(char *buffer, int max);
int keyboard_hashkey(void);
void keyboard_flush(void);

#endif