// utils.c

#include "utils.h"

int strcmp(const char *string1, const char *string2)
{
    while (*string1 && (*string1 == *string2)) {
        string1++;
        string2++;
    }

    return (unsigned char)*string1 - (unsigned char)*string2;
}