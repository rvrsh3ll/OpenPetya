// ata.c
// Advanced Technology Attachment
// Write for writing and reading specified sector.

#include "ata.h"
#include "io.h"
#include "types.h"

// Reference: https://wiki.osdev.org/ATA_PIO_Mode#Addressing_Modes
#define ATA_DATA        0x1F0
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LO      0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HI      0x1F5
#define ATA_REG_DRIVE   0x1F6
#define ATA_CMD         0x1F7

#define ATA_SR_BSY      0x80
#define ATA_SR_DRQ      0x08
#define ATA_SR_ERR      0x01

static void ata_wait(void)
{
    while (inb(ATA_CMD) & ATA_SR_BSY);
}

static int awa_wait_drq(void)
{

}

int ata_read(uint32_t lba, uint8_t count, uint8_t *buffer)
{

}

int ata_write(uint32_t lba, uint8_t count, const uint8_t *buffer)
{
    
}