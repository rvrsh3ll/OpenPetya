// ntfs_crypt.c

#include "ntfs_crypt.h"

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

}

static int read_salt(uint8_t salt[SALT_SIZE])
{

}

static int get_mft_lba(uint32_t partition_lba, uint32_t *mft_lba_out)
{

}

static int mtfs_mft_crypt(const char *password, uint32_t partition_lba, int encrypt)
{

}

int ntfs_mft_encrypt(const char *password, uint32_t partition_lba)
{

}

int ntfs_mft_decrypt(const char *password, uint32_t partition_lba)
{

}

int ntfs_generate_salt(void)
{

}