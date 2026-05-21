// state.h

#ifndef STATE_H
#define STATE_H

#include "types.h"

#define STATE_SECTOR 60
#define STATE_MAGIC 0x424F4F54UL

#define STATE_NOT_SETUP 0x00
#define STATE_ENCRYPTED 0x01

typedef struct {
    uint32_t magic;
    uint8_t state;
    uint8_t reserved[3];
    uint64_t disk_total_sectors; // set by installer, used by hidden_store
    uint8_t padding[496];
} __attribute__ ((packed)) stBootState;

/// @brief Read state from disk
/// @param  
/// @return 
uint8_t state_read(void);

/// @brief Write state into disk
/// @param new_state 
/// @return 
int state_write(uint8_t new_state);

/// @brief Read disk size
/// @param  
/// @return 
uint64_t state_read_disk_size(void);

#endif