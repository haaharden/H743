#ifndef OTA_IMAGE_H
#define OTA_IMAGE_H

#include "ff.h"
#include "ota_error_codes.h"
#include "ota_info.h"

#ifdef __cplusplus
extern "C" {
#endif

OTA_Update_Result_t OTA_Image_LoadHeaderFromFile(FIL *fp, Image_Info_t *out);
OTA_Update_Result_t OTA_Image_Validate(const Image_Info_t *info,
                                       uint32_t file_size,
                                       uint32_t max_payload_bytes);
/** MSP / Reset_Handler checks aligned with bootloader rules for destination slot. */
OTA_Update_Result_t OTA_Image_ValidatePayloadForSlot(const uint8_t payload_prefix_8[8],
                                                     uint32_t slot_flash_base,
                                                     uint32_t slot_flash_size);
OTA_Update_Result_t OTA_Image_LoadPayloadPrefixFromFile(FIL *fp,
                                                        uint32_t payload_offset,
                                                        uint8_t out_8[8]);

#ifdef __cplusplus
}
#endif

#endif /* OTA_IMAGE_H */
