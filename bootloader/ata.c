// ata.c
// Disk I/O via BIOS INT 13h Extended (through real-mode trampoline)

#include "ata.h"
#include "io.h"
#include "types.h"
#include "bootloader.h"

static uint8_t g_drive = 0x80;

void ata_set_drive(uint8_t drive)
{
    g_drive = drive;
}

// Parameter block layout at physical address 0x7E00:
// [0x7E00] uint8_t  function   (0x42=read, 0x43=write, 0x41=check)
// [0x7E01] uint8_t  drive
// [0x7E02] uint8_t  cf_result  (filled by trampoline: 1=error)
// [0x7E03] uint8_t  ah_result  (filled by trampoline: error code)
// [0x7E04] uint8_t  padding[12]
// [0x7E10] DAP[16 bytes]       (the actual disk address packet)

#define PARAM_BASE   ((volatile uint8_t *)0x7E00)
#define PARAM_FUNC   (PARAM_BASE + 0)
#define PARAM_DRIVE  (PARAM_BASE + 1)
#define PARAM_CF     (PARAM_BASE + 2)
#define PARAM_AH     (PARAM_BASE + 3)
#define DAP_BASE     ((volatile uint8_t *)0x7E10)

typedef struct __attribute__((packed))
{
    uint8_t  size;
    uint8_t  reserved;
    uint16_t count;
    uint16_t offset;
    uint16_t segment;
    uint64_t lba;
} DAP;

// Declared in entry.asm
extern void rm_call_int13(void);

#define BOUNCE_BUFFER      0x6000   // move it lower
#define BOUNCE_BUFFER_SIZE 4096     // 8 sectors max

static void setup_dap(uint32_t lba, uint8_t count)
{
    volatile DAP *dap = (volatile DAP *)DAP_BASE;
    dap->size = 0x10;
    dap->reserved = 0x00;
    dap->count = count;
    dap->segment = (uint16_t)(BOUNCE_BUFFER >> 4);
    dap->offset = (uint16_t)(BOUNCE_BUFFER & 0x0F);
    dap->lba = (uint64_t)lba;
}

static int call_int13(uint8_t func, uint32_t lba, uint8_t count, uint32_t flat_addr)
{
    if ((uint32_t)count * 512 > BOUNCE_BUFFER_SIZE)
    {
        vga_puts("call_int13: count too large!\n");
        return -1;
    }

    // for writes: copy to bounce buffer first
    if (func == 0x43)
    {
        uint8_t *src = (uint8_t *)flat_addr;
        uint8_t *dst = (uint8_t *)BOUNCE_BUFFER;
        for (uint32_t i = 0; i < (uint32_t)count * 512; i++)
            dst[i] = src[i];
    }

    *PARAM_FUNC = func;
    *PARAM_DRIVE = g_drive;
    *PARAM_CF = 0;
    *PARAM_AH = 0;

    setup_dap(lba, count);
    rm_call_int13();

    if (*PARAM_CF)
    {
        vga_puts("INT13 error AH=");
        vga_put_hex(*PARAM_AH);
        vga_puts(" LBA=");
        vga_put_hex(lba);
        vga_putchar('\n');
        return -1;
    }

    // for reads: copy from bounce buffer to destination
    if (func == 0x42)
    {
        uint8_t *src = (uint8_t *)BOUNCE_BUFFER;
        uint8_t *dst = (uint8_t *)flat_addr;
        for (uint32_t i = 0; i < (uint32_t)count * 512; i++)
            dst[i] = src[i];
    }

    return 0;
}

int ata_init(void)
{
    vga_puts("ATA: Checking INT 13h for drive ");
    vga_put_hex(g_drive);
    vga_putchar('\n');

    // use function 0x41 (check extensions), DAP not needed, just checks support
    *PARAM_FUNC = 0x41;
    *PARAM_DRIVE = g_drive;
    *PARAM_CF = 0;
    *PARAM_AH = 0;

    // handle this as a special case in trampoline
    static uint8_t scratch[512];
    if (call_int13(0x42, 0, 1, (uint32_t)scratch) != 0)
    {
        vga_puts("ATA: INT 13h read test failed!\n");
        return -1;
    }

    vga_puts("ATA: Ready.\n");
    return 0;
}

int ata_read(uint32_t lba, uint8_t count, uint8_t *buffer)
{
    return call_int13(0x42, lba, count, (uint32_t)buffer);
}

int ata_write(uint32_t lba, uint8_t count, const uint8_t *buffer)
{
    // Copy to bounce buffer first, then write
    uint8_t *dst = (uint8_t *)BOUNCE_BUFFER;
    uint32_t bytes = (uint32_t)count * 512;
    for (uint32_t i = 0; i < bytes; i++)
        dst[i] = buffer[i];

    return call_int13(0x43, lba, count, BOUNCE_BUFFER);
}