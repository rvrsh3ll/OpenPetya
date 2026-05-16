// ntfs_crypt.c

#include "ntfs_crypt.h"
#include "ata.h"
#include "bootloader.h"

static uint32_t prng_state = 0;

static void prng_seed(void)
{
    uint32_t val = 0;
    __asm__ volatile (
        "xor %%eax %%eax\n"
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

    for (int i = 0; i < SALT_SECTOR; i++)
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

static int ntfs_mft_crypt(const char *password, uint32_t partition_lba, int encrypt)
{

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
        
    }
}

int ntfs_mft_encrypt(const char *password, uint32_t partition_lba)
{
    
}

int ntfs_mft_decrypt(const char *password, uint32_t partition_lba)
{

}