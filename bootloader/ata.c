// ata.c
// Advanced Technology Attachment

#include "ata.h"

// Reference: https://wiki.osdev.org/ATA_PIO_Mode#Addressing_Modes
#define ATA_DATA 0x1F0
#define ATA_SECTOR_CNT 0x1F2
#define ATA_LBA_LO 0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HI 0x1F5
#define ATA_REG_DRIVE 0x1F6
#define ATA_CMD 0x1F7

