// ntfs_crypt.h

#ifndef NTFS_CRYPT_H
#define NTFS_CRYPT_H

#include "types.h"

#define SALT_SECTOR 62
#define SALT_SIZE 16

#define MFT_ENCRYPT_SECTORS 256

#define KDF_ITERATIONS 1000

int ntfs_mft_encrypt(const char *password, uint32_t partition_lba);

int ntfs_mft_decrypt(const char *password, uint32_t partition_lba);

int ntfs_generate_salt(void);

#endif