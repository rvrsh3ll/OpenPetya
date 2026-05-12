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
#define ATA_STATUS      0x1F7

#define ATA_SR_BSY      0x80
#define ATA_SR_DRQ      0x08
#define ATA_SR_ERR      0x01

static void ata_wait(void)
{
    while (inb(ATA_CMD) & ATA_SR_BSY);
}

static int ata_wait_drq(void)
{
    uint8_t status;
    for (int i = 0; i < 100000; i++)
    {
        status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR)
            return -1;
        if (status & ATA_SR_DRQ)
            return 0;
    }

    return -1;
}

int ata_read(uint32_t lba, uint8_t count, uint8_t *buffer)
{
    ata_wait();

    outb(ATA_REG_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_CNT, count);
    outb(ATA_LBA_LO, (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_CMD, 0x20);

    uint16_t *ptr = (uint16_t *)buffer; // In Real-Mode, 1 word = 2 bytes = 16 bits
    for (int s = 0; s < count; s++)
    {
        if (ata_wait_drq() != 0)
            return -1;

        for (int i = 0; i < 256; i++)
            ptr[i] = inw(ATA_DATA);

        ptr += 256;
    }

    return 0;
}

int ata_write(uint32_t lba, uint8_t count, const uint8_t buffer)
{
    ata_wait();

    outb(ATA_REG_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_CNT, count);
    outb(ATA_LBA_LO, (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_CMD, 0x30);

    const uint16_t *ptr = (const uint16_t *)buffer;
    for (int s = 0; s < count; s++)
    {
        if (ata_wait_drq() != 0)
            return -1;

        for (int i = 0; i < 256; i++)
            outw(ATA_DATA, ptr[i]);

        ptr += 256;
        outb(ATA_CMD, 0xE7);
        ata_wait();
    }

    return 0;
}