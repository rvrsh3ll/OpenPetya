// ntfs_crypt.h

#ifndef NTFS_CRYPT_H
#define NTFS_CRYPT_H

#include "types.h"

#define SALT_SECTOR 62
#define SALT_SIZE 16

#define MFT_ENCRYPT_SECTORS 256

#define KDF_ITERATIONS 1000

/// @brief Encrypt MFT
/// @param password Salsa20 encryption password
/// @param partition_lba LBA of partition
/// @return 
int ntfs_mft_encrypt(const char *password, uint32_t partition_lba);

/// @brief Decrypt MFT
/// @param password Salsa20 decryption password
/// @param partition_lba LBA of partition
/// @return 
int ntfs_mft_decrypt(const char *password, uint32_t partition_lba);

/// @brief Generate salt for MFT encryption
/// @param  
/// @return 
int ntfs_generate_salt(void);

#endif