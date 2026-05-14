#include "ota_image.h"

#include <stdio.h>
#include <string.h>

OTA_Update_Result_t OTA_Image_LoadHeaderFromFile(FIL *fp, Image_Info_t *out)
{
    FRESULT fr;
    UINT br;

    if ((fp == NULL) || (out == NULL)) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    fr = f_lseek(fp, 0U);
    if (fr != FR_OK) {
        return OTA_UPDATE_READIMAGE_ERROR;
    }

    fr = f_read(fp, out, sizeof(Image_Info_t), &br);
    if ((fr != FR_OK) || (br != sizeof(Image_Info_t))) {
        return OTA_UPDATE_READIMAGE_ERROR;
    }

    return OTA_OK;
}

OTA_Update_Result_t OTA_Image_Validate(const Image_Info_t *info,
                                       uint32_t file_size,
                                       uint32_t max_payload_bytes)
{
    uint32_t header_sz = (uint32_t)sizeof(Image_Info_t);
    uint32_t total_expect;

    if (info == NULL) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    if (info->magic != Image_INFO_MAGIC) {
        printf("OTA image bad magic 0x%08lX\r\n", (unsigned long)info->magic);
        return OTA_UPDATE_IMAGE_ERROR;
    }

    if ((info->image_size == 0U) || (info->image_size > max_payload_bytes)) {
        printf("OTA image_size invalid: %lu max=%lu\r\n",
               (unsigned long)info->image_size,
               (unsigned long)max_payload_bytes);
        return OTA_UPDATE_SIZE_ERROR;
    }

    if (info->image_size > (0xFFFFFFFFU - header_sz)) {
        return OTA_UPDATE_SIZE_ERROR;
    }

    total_expect = header_sz + info->image_size;
    if (file_size != total_expect) {
        printf("OTA file size mismatch: file=%lu expect=%lu\r\n",
               (unsigned long)file_size,
               (unsigned long)total_expect);
        return OTA_UPDATE_SIZE_ERROR;
    }

    return OTA_OK;
}

/* Same RAM windows as 03_Firmware/H743_Bootloader bootloader.c Bootloader_IsValidStack */
#define OTA_SP_DTCM_START    0x20000000UL
#define OTA_SP_DTCM_END      0x20020000UL
#define OTA_SP_AXI_START     0x24000000UL
#define OTA_SP_AXI_END       0x24080000UL
#define OTA_SP_SRAM123_START 0x30000000UL
#define OTA_SP_SRAM123_END   0x30048000UL
#define OTA_SP_SRAM4_START   0x38000000UL
#define OTA_SP_SRAM4_END     0x38010000UL

static int OTA_Image_StackPointerOk(uint32_t sp)
{
    if ((sp & 0x7U) != 0U) {
        return 0;
    }

    if ((sp > OTA_SP_DTCM_START) && (sp <= OTA_SP_DTCM_END)) {
        return 1;
    }

    if ((sp > OTA_SP_AXI_START) && (sp <= OTA_SP_AXI_END)) {
        return 1;
    }

    if ((sp > OTA_SP_SRAM123_START) && (sp <= OTA_SP_SRAM123_END)) {
        return 1;
    }

    if ((sp > OTA_SP_SRAM4_START) && (sp <= OTA_SP_SRAM4_END)) {
        return 1;
    }

    return 0;
}

OTA_Update_Result_t OTA_Image_ValidatePayloadForSlot(const uint8_t payload_prefix_8[8],
                                                     uint32_t slot_flash_base,
                                                     uint32_t slot_flash_size)
{
    uint32_t initial_sp;
    uint32_t reset_handler;
    uint32_t real_pc;

    if (payload_prefix_8 == NULL) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    if ((slot_flash_base == 0U) || (slot_flash_size == 0U)) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    memcpy(&initial_sp, &payload_prefix_8[0], sizeof(initial_sp));
    memcpy(&reset_handler, &payload_prefix_8[4], sizeof(reset_handler));

    if (OTA_Image_StackPointerOk(initial_sp) == 0) {
        printf("OTA invalid initial SP: 0x%08lX\r\n", (unsigned long)initial_sp);
        return OTA_UPDATE_IMAGE_ERROR;
    }

    if ((reset_handler & 1U) == 0U) {
        printf("OTA invalid reset handler (Thumb): 0x%08lX\r\n",
               (unsigned long)reset_handler);
        return OTA_UPDATE_IMAGE_ERROR;
    }

    real_pc = reset_handler & 0xFFFFFFFEUL;

    if ((real_pc < slot_flash_base) ||
        (real_pc >= (slot_flash_base + slot_flash_size))) {
        printf("OTA invalid reset handler: 0x%08lX\r\n",
               (unsigned long)reset_handler);
        return OTA_UPDATE_IMAGE_ERROR;
    }

    return OTA_OK;
}

OTA_Update_Result_t OTA_Image_LoadPayloadPrefixFromFile(FIL *fp,
                                                        uint32_t payload_offset,
                                                        uint8_t out_8[8])
{
    FRESULT fr;
    UINT br;

    if ((fp == NULL) || (out_8 == NULL)) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    fr = f_lseek(fp, payload_offset);
    if (fr != FR_OK) {
        return OTA_UPDATE_READIMAGE_ERROR;
    }

    fr = f_read(fp, out_8, 8U, &br);
    if ((fr != FR_OK) || (br != 8U)) {
        return OTA_UPDATE_READIMAGE_ERROR;
    }

    return OTA_OK;
}
