// ata.h
// Advanced Technology Attachment
// Write for writing and reading specified sector.

#ifndef ATA_H
#define ATA_H

#include "types.h"

int ata_read(uint32_t lba, uint8_t count, uint8_t *buffer);
int ata_write(uint32_t lba, uint8_t count, const uint8_t *buffer);

#endif