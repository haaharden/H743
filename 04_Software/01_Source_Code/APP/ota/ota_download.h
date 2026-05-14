#ifndef OTA_DOWNLOAD_H
#define OTA_DOWNLOAD_H

#include "ff.h"
#include "ota_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

OTA_Update_Result_t OTA_EraseExternalFlash(uint32_t address, uint32_t length);

OTA_Update_Result_t OTA_EraseW25Aligned(uint32_t address, uint32_t length);

OTA_Update_Result_t OTA_EraseFirmwareSlot(uint32_t slot_id);

/**
 * Stream payload from FatFS @ file_payload_offset into external flash @ dst_addr.
 * Optional readback compare per chunk; final CRC32 over staged payload vs expected_crc32.
 *
 * @param dst_region_size max span [dst_addr, dst_addr+payload) allowed (e.g. OTA_NEW_FW_SIZE when dst_addr == OTA_NEW_FW_ADDR)
 */
OTA_Update_Result_t OTA_StagePayloadFromFile(FIL *fp,
                                           uint32_t file_payload_offset,
                                           uint32_t payload_size,
                                           uint32_t dst_addr,
                                           uint32_t dst_region_size,
                                           uint32_t expected_crc32);

/**
 * Stream complete file (header + firmware payload) into external flash firmware slot base.
 */
OTA_Update_Result_t OTA_StageFullFirmwarePkgFromFile(FIL *fp, uint32_t slot_id);

#ifdef __cplusplus
}
#endif

#endif /* OTA_DOWNLOAD_H */
