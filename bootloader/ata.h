// ata.h

#ifndef ATA_H
#define ATA_H

#include "types.h"

void ata_set_drive(uint8_t drive);
int  ata_init(void);
int  ata_read(uint32_t lba, uint8_t count, uint8_t *buffer);
int  ata_write(uint32_t lba, uint8_t count, const uint8_t *buffer);

#endif