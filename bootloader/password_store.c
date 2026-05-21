// password_store.c

#include "password_store.h"
#include "ata.h"
#include "bootloader.h"

static uint8_t sector_buffer[512];

int pwstore_read(char *buffer, int max_len)
{
    if (ata_read(PW_SECTOR, 1, sector_buffer) != 0)
    {
        vga_puts("pwstore_read: ATA read failed\n");
        return -1;
    }

    stPasswordStore *ps = (stPasswordStore *)sector_buffer;
    if (ps->magic != PW_MAGIC)
    {
        vga_puts("pwstore_read: no password found in sector 59\n");
        return -1;
    }

    if (ps->len == 0 || ps->len > PW_MAX_LEN)
    {
        vga_puts("pwstore_read: invalid password length\n");
        return -1;
    }

    int copy_len = ps->len < max_len - 1 ? ps->len : max_len - 1;
    for (int i = 0; i < copy_len; i++)
        buffer[i] = ps->password[i];

    buffer[copy_len] = '\0';

    return 0;
}

int pwstore_erase(void)
{
    for (int pass = 0; pass < 3; pass++)
    {
        for (int i = 0; i < 512; i++)
            sector_buffer[i] = (pass == 1 ? 0xFF : 0x00);

        if (ata_write(PW_SECTOR, 1, sector_buffer) != 0)
        {
            vga_puts("pwstore_erase: write failed\n");
            return -1;
        }
    }

    vga_puts("\tPassword sector erased (3 passes).\n");

    return 0;
}