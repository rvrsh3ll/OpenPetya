// ntfs.c

#include "ata.h"
#include "ntfs.h"
#include "bootloader.h"

static uint8_t mft_buffer[1024];
static uint8_t vbr_buffer[512];

static void apply_fixup(uint8_t *entry, int size)
{
    MFT_Entry *hdr = (MFT_Entry *)entry;
    uint16_t *update = (uint16_t *)(entry + hdr->update_seq_offset);
    uint16_t seq = update[0];

    int sectors = size / 512;

    for (int i = 0; i < sectors; i++)
    {
        uint16_t *end = (uint16_t *)(entry + 510 + i * 512);
        if (*end != seq)
        {
            vga_puts("Fixup mismatch at sector ");
            vga_put_dec(i);
            vga_putchar('\n');
        }

        *end = update[i+1];
    }
}

static void print_utf16(uint16_t *s, int len)
{
    for (int i = 0; i < len; i++)
    {
        char c = (s[i] < 128) ? (char)s[i] : '?';
        vga_putchar(c);
    }
}

static void dump_mft_entry(uint32_t entry_num, uint32_t mft_lba, uint8_t sectors_per_record)
{
    uint32_t lba = mft_lba + entry_num * sectors_per_record;
    if (ata_read(lba, sectors_per_record, mft_buffer) != 0)
    {
        vga_puts("\tATA read error\n");
        return;
    }

    MFT_Entry *entry = (MFT_Entry *)mft_buffer;
    
    if (entry->signature[0] != 'F' || entry->signature[1] != 'I' || entry->signature[2] != 'L' || entry->signature[3] != 'E')
    {
        vga_puts("Not a valid FILE record -> skip!\n");
        return;
    }

    apply_fixup(mft_buffer, sectors_per_record * 512);

    vga_puts("Entry ");
    vga_put_dec(entry_num);
    vga_puts(": flags=");
    vga_put_hex(entry->flags);

    if (!(entry->flags & MFT_FLAGS_IN_USE))
    {
        vga_puts(" [deleted]\n");
        return;
    }

    if (entry->flags & MFT_FLAGS_DIRECTORY)
        vga_puts(" [DIR]");
    else
        vga_puts(" [FILE]");

    uint8_t *ptr = mft_buffer + entry->attr_offset;
    while (ptr < mft_buffer + entry->used_size)
    {
        Attr_Header *attr = (Attr_Header *)ptr;

        if (attr->type == ATTR_END || attr->length == 0)
            break;

        if (attr->type == ATTR_FILE_NAME && !attr->non_resident)
        {
            Attr_Resident *res = (Attr_Resident *)(ptr + sizeof(Attr_Header));
            Attr_FileName *fn = (Attr_FileName *)(ptr + res->value_offset);

            if (fn->namespace == 1 || fn->namespace == 3)
            {
                vga_puts("name=");
                print_utf16(fn->name, fn->name_length);
                vga_puts("size=");
                vga_put_dec((uint32_t)fn->real_size);
            }

            (void)res;
        }

        ptr += attr->length;
    }

    vga_putchar('\n');
}

void ntfs_read_mft(uint32_t partition_lba)
{
    vga_puts("NTFS MFT Read\n\n");

    vga_puts("Reading VBR at LBA ");
    vga_put_dec(partition_lba);
    vga_puts("...\n");

    if (ata_read(partition_lba, 1, vbr_buffer) != 0)
    {
        vga_puts("Failed to read VBR!\n");
        return;
    }

    NTFS_VBR *vbr = (NTFS_VBR *)vbr_buffer;

    if (vbr->oem_id[0] != 'N' || vbr->oem_id[1] != 'T' || vbr->oem_id[2] != 'F' || vbr->oem_id[3] != 'S')
    {
        vga_puts("Invalid NTFS partition!.");
        vga_putchar('\n');
        for (int i = 0; i < 16; i++)
        {
            vga_put_hex(vbr_buffer[i]);
            vga_putchar(' ');
        }
        vga_putchar('\n');
        return;
    }

    vga_puts("OEM: ");
    for (int i = 0; i < 8; i++)
        vga_putchar(vbr->oem_id[i]);

    vga_putchar('\n');

    vga_puts("Bytes/sector: ");
    vga_put_dec(vbr->bytes_per_sector);
    vga_putchar('\n');

    vga_puts("Sectors/cluster: ");
    vga_put_dec(vbr->sectors_per_cluster);
    vga_putchar('\n');

    vga_puts("MFT cluster: ");
    vga_put_hex((uint32_t)vbr->mft_cluster);
    vga_putchar('\n');

    uint32_t mft_lba = partition_lba + (uint32_t)(vbr->mft_cluster * vbr->sectors_per_cluster);

    vga_puts("MFT LBA: ");
    vga_put_hex(mft_lba);
    vga_putchar('\n');

    uint32_t bytes_per_record;
    if (vbr->clusters_per_mft_record < 0)
    {
        bytes_per_record = 1u << (uint8_t)(-vbr->clusters_per_mft_record);
    }
    else
    {
        bytes_per_record = (uint32_t)vbr->clusters_per_mft_record * vbr->sectors_per_cluster * vbr->bytes_per_sector;
    }

    uint8_t sectors_per_record = bytes_per_record / 512;

    vga_puts("Bytes/record: ");
    vga_put_dec(bytes_per_record);
    vga_putchar('\n');

    vga_puts("Entries:\n");
    const char *system_names[] =
    {
        "$MFT",
        "$MFTMirr",
        "$LogFile",
        "$Volume",
        "$AttrDef",
        ".",
        "$Bitmap",
        "$Boot",
        "$BadClus",
        "$Secure",
        "$UpCase",
        "$Extend",
    };

    for (int i = 0; i < sizeof(system_names)/sizeof(*system_names); i++)
    {
        vga_puts(system_names[i]);
        vga_puts("->");
        dump_mft_entry(i, mft_lba, sectors_per_record);
    }

    vga_putchar('\n');

    vga_puts("User's files\n");
    for (int i = 12; i < 32; i++)
    {
        dump_mft_entry(i, mft_lba, sectors_per_record);
    }

    vga_puts("\nAction successfully.\n");
}