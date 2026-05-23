// ntfs_crypt.c

#include "ntfs_crypt.h"
#include "ata.h"
#include "kdf.h"
#include "salsa20.h"
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
    static uint8_t sector[512];
    if (ata_read(SALT_SECTOR, 1, sector) != 0)
        return -1;

    // Check magic at bytes 0–3
    if (sector[0] != 0x53 || sector[1] != 0x41 ||
        sector[2] != 0x4C || sector[3] != 0x54)
    {
        vga_puts("Salt sector: invalid magic!\n");
        return -1;
    }

    // Read salt from bytes 4 onward
    for (int i = 0; i < SALT_SIZE; i++)
        salt[i] = sector[4 + i];

    return 0;
}

static int get_mft_lba(uint32_t partition_lba, uint32_t *mft_lba_out)
{
    static uint8_t vbr[512];
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

    static uint8_t sector_buffer[512];
    static uint8_t out_buffer[512];

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
    static uint8_t sector[512] = { 0 };

    // Magic marker FIRST at bytes 0–3
    sector[0] = 0x53;  // 'S'
    sector[1] = 0x41;  // 'A'
    sector[2] = 0x4C;  // 'L'
    sector[3] = 0x54;  // 'T'

    prng_seed();

    // Salt at bytes 4 onward
    for (int i = 0; i < SALT_SIZE; i += 4)
    {
        uint32_t r = prng_next();
        sector[4 + i + 0] = (uint8_t)r;
        sector[4 + i + 1] = (uint8_t)(r >> 8);
        sector[4 + i + 2] = (uint8_t)(r >> 16);
        sector[4 + i + 3] = (uint8_t)(r >> 24);
    }

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
    
    static uint8_t sector_buffer[512];
    static uint8_t out_buffer[512];
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

    static uint8_t sector_buffer[512];
    static uint8_t out_buffer[512];

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

static const uint8_t KNOWN_PLAIN[32] = {
    'B','O','O','T','L','O','A','D',
    'E','R','V','E','R','I','F','Y',
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

    salsa20_init(&ctx, key, nonce, 0);

    salsa20_encrypt(&ctx, KNOWN_PLAIN, tag, TAG_SIZE);
}

int validate_save_tag(const uint8_t key[32])
{
    static uint8_t sector[512] = { 0 };

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
    static uint8_t sector[512] __attribute__((aligned(16)));

    if (ata_read(VALIDATE_SECTOR, 1, sector) != 0)
    {
        vga_puts("validate_check_key: read failed\n");
        return 0;
    }

    if (sector[0] != VALIDATE_MAGIC_0 || sector[1] != VALIDATE_MAGIC_1 || sector[2] != VALIDATE_MAGIC_2 || sector[3] != VALIDATE_MAGIC_3)
    {
        vga_puts("Validate sector not initialized!");
        return 0;
    }

    uint8_t expected_tag[TAG_SIZE] __attribute__((aligned(16)));;
    compute_tag(key, expected_tag);

    uint8_t diff = 0;
    for (int i = 0; i < TAG_SIZE; i++)
        diff |= expected_tag[i] ^ sector[i + 4];

    return diff == 0; // 0 = all bytes match = correct key
}