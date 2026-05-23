// ntfs.h

#ifndef NTFS_H
#define NTFS_H

#include "types.h"

#define ATTR_STANDARD_INFO  0x10
#define ATTR_FILE_NAME      0x30
#define ATTR_DATA           0x80
#define ATTR_END            0xFFFFFFFF

#define MFT_FLAGS_IN_USE    0x01
#define MFT_FLAGS_DIRECTORY 0x02

// VBR (Volume Boot Record), the first sector of NTFS partition
typedef struct __attribute__((packed))
{
    uint8_t jump[3];
    uint8_t oem_id[8];

    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;

    uint16_t reserved_sectors;
    uint8_t zeros1[3];

    uint16_t unused1;
    uint8_t media_descriptor;
    uint16_t zeros2;

    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t unused2;
    uint32_t unused3;

    uint64_t total_sectors;
    uint64_t mft_cluster;
    uint64_t mft_mirror_cluster;

    int8_t clusters_per_mft_record;
    uint8_t reserved0[3];

    int8_t clusters_per_index_record;
    uint8_t reserved1[3];

    uint64_t volume_serial;
    uint32_t checksum;

} NTFS_VBR;

// MFT Entry header
typedef struct {
    uint8_t signature[4];
    uint16_t update_seq_offset;
    uint16_t update_seq_count;
    uint64_t log_seq_number;
    uint16_t sequence_number;
    uint16_t hard_link_count;
    uint16_t attr_offset;
    uint16_t flags;
    uint32_t used_size;
    uint32_t allocated_size;
    uint64_t base_record;
    uint16_t next_attr_id;
    uint8_t reserved[2];
    uint32_t record_number;
} __attribute__((packed)) MFT_Entry;

// Attribute header
typedef struct {
    uint32_t type;
    uint32_t length;
    uint8_t non_resident;
    uint8_t name_length;
    uint16_t name_offset;
    uint16_t flags;
    uint16_t attr_id;
} __attribute__((packed)) Attr_Header;

typedef struct {
    uint32_t value_length;
    uint16_t value_offset;
    uint16_t flags;
} __attribute__((packed)) Attr_Resident;

typedef struct {
    uint64_t parent_ref;
    uint64_t created_time;
    uint64_t modified_time;
    uint64_t mft_modified_time;
    uint64_t accessed_time;
    uint64_t allocated_size;
    uint64_t real_size;
    uint32_t flags;
    uint32_t reparse_value;
    uint8_t name_length;
    uint8_t namespace;
    uint16_t name[1];
} __attribute__((packed)) Attr_FileName;

void ntfs_read_mft(uint32_t partition_lba);

#endif