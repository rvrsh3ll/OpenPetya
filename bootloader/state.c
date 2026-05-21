// state.c

#include "state.h"
#include "ata.h"
#include "bootloader.h"

static uint8_t buffer[512];

uint8_t state_read(void)
{
    if (ata_read(STATE_SECTOR, 1, buffer) != 0)
        return STATE_NOT_SETUP;

    stBootState *state = (stBootState *)buffer;
    if (state->magic != STATE_MAGIC)
        return STATE_NOT_SETUP;

    return state->state;
}

int state_write(uint8_t new_state)
{
    ata_read(STATE_SECTOR, 1, buffer);
    stBootState *state = (stBootState *)buffer;
    state->magic = STATE_MAGIC;
    state->state = new_state;

    if (ata_write(STATE_SECTOR, 1, buffer) != 0)
        return -1;

    return 0;
}

uint64_t state_read_disk_size(void)
{
    if (ata_read(STATE_SECTOR, 1, buffer) != 0)
        return 0;

    vga_puts("ata_read: OK\n");

    stBootState *state = (stBootState *)buffer;
    if (state->magic != STATE_MAGIC)
    {
        vga_put_hex(state->magic);
        vga_putchar('\n');

        return 0;
    }

    vga_put_hex(state->magic);
    vga_putchar('\n');

    return state->disk_total_sectors;
}