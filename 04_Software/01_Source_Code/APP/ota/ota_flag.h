/*
 * External OTA duplicated flag mirrors (dual copy, seq picks winner). Flag CRC skipped by design for now.
 */
#ifndef OTA_FLAG_H
#define OTA_FLAG_H

#include "ota_error_codes.h"
#include "ota_info.h"

OTA_Update_Result_t OTA_GetValidFlag(OTA_Flag_t *flag);

/**
 * Write @p flag to inactive mirror; caller must bump @p seq over last merged snapshot.
 */
OTA_Update_Result_t OTA_FlagWriteMerged(const OTA_Flag_t *flag);

/**
 * After bootloader left OTA_INSTALLED, APP waits its trial delay then confirms.
 * Promotes staging_slot to committed, clears staging/boot_count.
 */
OTA_Update_Result_t OTA_AppConfirmTrialSuccess(void);

#endif /* OTA_FLAG_H */
