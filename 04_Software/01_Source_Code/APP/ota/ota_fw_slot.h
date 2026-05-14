/**
 * External W25Q OTA firmware slot IDs (alternate A/B staging).
 * Keep addresses in sync with SYSTEM/system_storge.h and bootloader Core/Inc/ota_fw_layout.h
 */
#ifndef OTA_FW_SLOT_H
#define OTA_FW_SLOT_H

#include <stdint.h>

#include "system_storge.h"

#define OTA_FW_SLOT_NONE        0U
#define OTA_FW_SLOT_A           1U
#define OTA_FW_SLOT_B           2U

static inline uint32_t OTA_Fw_Slot_Size(uint32_t slot_id)
{
    (void)slot_id;
    return OTA_NEW_FW_SIZE;
}

static inline uint32_t OTA_Fw_Slot_BaseAddr(uint32_t slot_id)
{
    if (slot_id == OTA_FW_SLOT_A) {
        return OTA_NEW_FW_ADDR;
    }

    if (slot_id == OTA_FW_SLOT_B) {
        return OTA_OLD_FW_ADDR;
    }

    return 0U;
}

static inline uint32_t OTA_Fw_OppositeSlot(uint32_t slot_id)
{
    if (slot_id == OTA_FW_SLOT_A) {
        return OTA_FW_SLOT_B;
    }

    if (slot_id == OTA_FW_SLOT_B) {
        return OTA_FW_SLOT_A;
    }

    return OTA_FW_SLOT_NONE;
}

/**
 * Staging uses the firmware slot opposite the committed golden image.
 * If committed is NONE (minimal factory), stage into slot B first.
 */
static inline uint32_t OTA_Fw_PickStagingSlot(uint32_t committed_slot_id)
{
    if (committed_slot_id == OTA_FW_SLOT_NONE) {
        return OTA_FW_SLOT_B;
    }

    return OTA_Fw_OppositeSlot(committed_slot_id);
}

#endif /* OTA_FW_SLOT_H */
