// keyboard.h

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "bootloader.h"

void keyboard_int(void);
char keyboard_getchar(void);
void keyboard_readline(char *buffer, int max);
int keyboard_hashkey(void);

#endif