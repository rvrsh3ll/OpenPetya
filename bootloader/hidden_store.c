// hidden_store.c

#include "hidden_store.h"
#include "ata.h"
#include "types.h"
#include "bootloader.h"

static uint8_t sector_buffer[512];
static uint8_t mft_buffer[512];
static uint64_t disk_end_lba = 0;

static uint32_t header_lba(void)
{
    return (uint32_t)(disk_end_lba - 2);
}

static uint32_t vbr_backup_lba(void)
{
    return (uint32_t)(disk_end_lba - 3);
}

static uint32_t mft_backup_start_lba(void)
{
    return (uint32_t)(disk_end_lba - 3 - MFT_BACKUP_SECTORS);
}

static uint32_t get_mft_lba(uint32_t partition_lba)
{
    vga_puts("get_mft_lba: reading VBR at LBA ");
    vga_put_dec(partition_lba);
    vga_putchar('\n');

    if (ata_read(partition_lba, 1, sector_buffer) != 0)
    {
        vga_puts("get_mft_lba: VBR read failed!\n");
        return 0;
    }

    // Check NTFS signature at offset 3
    vga_puts("OEM: ");
    for (int i = 3; i < 11; i++)
        vga_putchar(sector_buffer[i]);
    vga_putchar('\n');

    if (sector_buffer[3] != 'N' || sector_buffer[4] != 'T' ||
        sector_buffer[5] != 'F' || sector_buffer[6] != 'S')
    {
        vga_puts("get_mft_lba: Not NTFS at this LBA!\n");
        vga_puts("First 16 bytes: ");
        for (int i = 0; i < 16; i++)
        {
            vga_put_hex(sector_buffer[i]);
            vga_putchar(' ');
        }
        vga_putchar('\n');
        return 0;
    }

    uint8_t spc = sector_buffer[13];
    vga_puts("sectors_per_cluster: ");
    vga_put_dec(spc);
    vga_putchar('\n');

    uint64_t mft_cluster = 0;
    for (int i = 0; i < 8; i++)
        mft_cluster |= (uint64_t)sector_buffer[i + 48] << (i * 8);

    vga_puts("mft_cluster: ");
    vga_put_hex((uint32_t)mft_cluster);
    vga_putchar('\n');

    uint32_t mft_lba = (uint32_t)(partition_lba + mft_cluster * spc);
    vga_puts("mft_lba: ");
    vga_put_dec(mft_lba);
    vga_putchar('\n');

    return mft_lba;
}

int hidden_store_init(uint64_t disk_sectors)
{
    disk_end_lba = disk_sectors - 1;
    return 0;
}

int hidden_backup_mft(uint32_t partition_lba)
{
    vga_puts("Backing up MFT to hidden area...\n");

    uint32_t mft_lba = get_mft_lba(partition_lba);
    if (mft_lba == 0)
    {
        vga_puts("Cannot find MFT!\n");
        return -1;
    }

    vga_puts("MFT at LBA: ");
    vga_put_dec(mft_lba);
    vga_putchar('\n');

    vga_puts("Backup LBA: ");
    vga_put_dec(mft_backup_start_lba());
    vga_putchar('\n');

    // Backup MFT sectors
    for (uint32_t i = 0; i < MFT_BACKUP_SECTORS; i++)
    {
        if (ata_read(mft_lba + i, 1, mft_buffer) != 0)
        {
            vga_puts("MFT read error at sector ");
            vga_put_dec(i);
            vga_putchar('\n');

            return -1;
        }

        if (ata_write(mft_backup_start_lba() + i, 1, mft_buffer) != 0)
        {
            vga_puts("Hidden write eror at sector: ");
            vga_put_dec(i);
            vga_putchar('\n');

            return -1;
        }
    }

    // Backup VBR
    if (ata_read(partition_lba, 1, sector_buffer) != 0)
        return -1;

    if (ata_write(vbr_backup_lba(), 1, sector_buffer) != 0)
        return -1;

    HiddenHeader hdr = { 0 };
    hdr.magic = HIDDEN_MAGIC;
    hdr.partition_lba = partition_lba;
    hdr.mft_lba = mft_lba;
    hdr.mft_sector_count = MFT_BACKUP_SECTORS;
    hdr.disk_total_sectors = disk_end_lba + 1;

    static uint8_t hdr_sector[512] = { 0 };
    for (uint32_t i = 0; i < sizeof(HiddenHeader); i++)
        hdr_sector[i] = ((uint8_t *)&hdr)[i];

    if (ata_write(header_lba(), 1, hdr_sector) != 0)
        return -1;

    vga_puts("MFT backup successfully.\n");

    return 0;
}

int hidden_restore_mft(uint32_t partition_lba)
{
    vga_puts("Restoring MFT from hidden area...\n");

    HiddenHeader hdr;
    if (hidden_read_header(&hdr) != 0)
    {
        vga_puts("Cannot read hidden header!\n");
        return -1;
    }

    vga_puts("Restoring...\n");
    vga_put_dec(hdr.mft_sector_count);
    vga_puts("MFT sectors to LBA ");
    vga_put_dec(hdr.mft_lba);
    vga_putchar('\n');

    // Restore MFT sectors
    for (uint32_t i = 0; i < hdr.mft_sector_count; i++)
    {
        if (ata_read(mft_backup_start_lba() + i, 1, mft_buffer) != 0)
        {
            vga_puts("Backup read error!\n");
            return -1;
        }

        if (ata_write(hdr.mft_lba + i, 1, mft_buffer) != 0)
        {
            vga_puts("MFT write error!\n");
            return -1;
        }
    }

    // Restore VBR
    if (ata_read(vbr_backup_lba(), 1, sector_buffer) != 0)
        return -1;

    if (ata_write(partition_lba, 1, sector_buffer) != 0)
        return -1;

    vga_puts("MFT was restored!\n");

    return 0;
}

int hidden_wipe(void)
{
    vga_puts("Wiping hidden area...\n");

    static uint8_t zero[512] = { 0 };

    for (uint32_t i = 0; i < MFT_BACKUP_SECTORS; i++)
        ata_write(mft_backup_start_lba() + i, 1, zero);

    ata_write(vbr_backup_lba(), 1, zero);
    ata_write(header_lba(), 1, zero);

    vga_puts("Hidden area is wiped.\n");

    return 0;
}

int hidden_read_header(HiddenHeader *hdr)
{
    static uint8_t state[512];
    if (ata_read(60, 1, state) != 0)
        return -1;

    uint64_t total = 0;
    for (int i = 0; i < 8; i++)
        total |= (uint64_t)state[i + 8] << (i * 8);

    if (total == 0)
        return -1;

    disk_end_lba = total - 1;

    if (ata_read(header_lba(), 1, sector_buffer) != 0)
        return -1;

    HiddenHeader *header = (HiddenHeader *)sector_buffer;
    if (header->magic != HIDDEN_MAGIC)
        return -1;

    *hdr = *header;

    return 0;
}