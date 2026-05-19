// utils.h

#ifndef UTILS_H
#define UTILS_H

#include "types.h"

int strcmp(const char *string1, const char *string2);
void sleep(uint32_t ms);
void zero_buffer(char *buffer, int len);

#endif