// utils.c

#include "utils.h"
#include "types.h"
#include "io.h"

// string comparison
int strcmp(const char *string1, const char *string2)
{
    while (*string1 && (*string1 == *string2))
    {
        string1++;
        string2++;
    }

    return (unsigned char)*string1 - (unsigned char)*string2;
}

// sleep for specified miliseconds
void sleep(uint32_t ms)
{
    // 1193182 Hz / 1000 = 1193 ticks per millisecond
    uint16_t ticks_per_ms = 1193;

    for (uint32_t i = 0; i < ms; i++)
    {
        // Set PIT to Mode 0 (Interrupt on Terminal Count)
        // Binary, Mode 0, LSB then MSB, Channel 0
        outb(0x43, 0x30); 

        // Send the frequency divider
        outb(0x40, (uint8_t)(ticks_per_ms & 0xFF));        // Low byte
        outb(0x40, (uint8_t)((ticks_per_ms >> 8) & 0xFF)); // High byte

        // Poll the PIT until it reaches zero
        // In Mode 0, the output pin goes high when counting is done
        uint8_t status = 0;
        while (!(status & 0x80))
        {
            // Command 0xE2: Read-back command for channel 0
            outb(0x43, 0xE2);
            status = inb(0x40);
        }
    }
}