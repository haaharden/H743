/**
 * Legacy internal-flash BootInfo staging (BOOT_INFO sector). Deprecated: OTA uses
 * external `OTA_Flag_t` mirrors + dual package slots (`system_storge.h`).
 */
#ifndef OTA_BOOT_SHARED_H
#define OTA_BOOT_SHARED_H

#include <stdint.h>

#define BOOT_INFO_MAGIC          0x424F4F54UL
#define BOOT_INFO_ADDR           0x081E0000UL
#define BOOT_FLASH_WORD_SIZE     32U

typedef enum {
    BOOT_SLOT_NONE = 0,
    BOOT_SLOT_APP1 = 1,
    BOOT_SLOT_APP2 = 2,
} BootSlot_t;

typedef enum {
    BOOT_UPDATE_IDLE = 0,
    BOOT_UPDATE_STAGED = 1,
    BOOT_UPDATE_INSTALLING = 2,
    BOOT_UPDATE_DONE = 3,
    BOOT_UPDATE_FAIL = 4,
} BootUpdateState_t;

typedef struct {
    uint32_t magic;
    uint32_t active_slot;
    uint32_t target_slot;
    uint32_t update_state;
    uint32_t image_size;
    uint32_t image_ext_addr;
    uint32_t last_result;
} BootInfo_t;

#endif /* OTA_BOOT_SHARED_H */
