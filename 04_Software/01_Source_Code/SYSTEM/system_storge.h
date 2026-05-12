#ifndef SYSTEM_STORGE_H
#define SYSTEM_STORGE_H

#include <stdint.h>

#define EXT_FLASH_SIZE          0x02000000UL   // 32MB

#define RAWFS_BASE_ADDR         0x00000000UL
#define RAWFS_SIZE              0x01000000UL   // 16MB

#define OTA_NEW_FW_ADDR         0x01000000UL
#define OTA_NEW_FW_SIZE         0x00200000UL   // 2MB

#define OTA_OLD_FW_ADDR         0x01200000UL
#define OTA_OLD_FW_SIZE         0x00200000UL   // 2MB，可选

#define APP1_FLASH_ADDR         0x08020000UL
#define APP1_FLASH_SIZE         0x000E0000UL
#define APP1_FLASH_END_ADDR     (APP1_FLASH_ADDR + APP1_FLASH_SIZE)

#define APP2_FLASH_ADDR         0x08100000UL
#define APP2_FLASH_SIZE         0x000E0000UL
#define APP2_FLASH_END_ADDR     (APP2_FLASH_ADDR + APP2_FLASH_SIZE)

// #define OTA_FLAG_ADDR           0x01FF0000UL
// #define OTA_FLAG_SIZE           0x00010000UL   // 64KB

#define BOOT_INFO_MAGIC       0x424F4F54UL  // 'BOOT'
#define BOOT_INFO_ADDR        0x081E0000UL

#define EXT_FW_STAGING_ADDR   0x00000000UL

typedef enum {
    BOOT_SLOT_NONE = 0,
    BOOT_SLOT_APP1 = 1,
    BOOT_SLOT_APP2 = 2
} BootSlot_t;

typedef enum {
    BOOT_UPDATE_IDLE = 0,
    BOOT_UPDATE_STAGED = 1,
    BOOT_UPDATE_INSTALLING = 2,
    BOOT_UPDATE_DONE = 3,
    BOOT_UPDATE_FAIL = 4
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

#define OTA_RUNNING_SLOT        BOOT_SLOT_APP1
#define OTA_TARGET_SLOT         BOOT_SLOT_APP2
#define OTA_TARGET_FLASH_ADDR   APP2_FLASH_ADDR
#define OTA_TARGET_FLASH_SIZE   APP2_FLASH_SIZE
#define OTA_TARGET_FLASH_END_ADDR APP2_FLASH_END_ADDR
#define OTA_TARGET_NAME         "APP2"

#endif