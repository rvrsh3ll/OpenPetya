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
    static uint8_t buf[512];

    // Read current sector to preserve disk_total_sectors
    if (ata_read(STATE_SECTOR, 1, buf) != 0)
    {
        vga_puts("state_write: read failed\n");
        return -1;
    }

    stBootState *state = (stBootState *)buf;

    vga_puts("state_write: before write, disk_sectors=");
    vga_put_dec((uint32_t)state->disk_total_sectors);
    vga_putchar('\n');

    state->magic = STATE_MAGIC;
    state->state = new_state;

    if (ata_write(STATE_SECTOR, 1, buf) != 0)
    {
        vga_puts("state_write: write failed\n");
        return -1;
    }

    // Verify the write actually stuck
    static uint8_t verify[512];
    if (ata_read(STATE_SECTOR, 1, verify) != 0)
    {
        vga_puts("state_write: verify read failed\n");
        return -1;
    }

    stBootState *v = (stBootState *)verify;
    vga_puts("state_write verify: magic=");
    vga_put_hex(v->magic);
    vga_puts(" state=");
    vga_put_hex(v->state);
    vga_putchar('\n');

    if (v->magic != STATE_MAGIC || v->state != new_state)
    {
        vga_puts("state_write: verify MISMATCH!\n");
        return -1;
    }

    return 0;
}

uint64_t state_read_disk_size(void)
{
    if (ata_read(STATE_SECTOR, 1, buffer) != 0)
    {
        vga_puts("ATA read FAILED\n");
        return 0;
    }

    vga_puts("ata_read OK, magic=");

    stBootState *state = (stBootState *)buffer;
    vga_put_hex(state->magic);
    vga_puts(" disk_sectors=");
    vga_put_dec((uint32_t)state->disk_total_sectors);
    vga_putchar('\n');

    if (state->magic != STATE_MAGIC)
        return 0;

    return state->disk_total_sectors;
}