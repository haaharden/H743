/*
 * U disk OTA: Image_Info_t + raw payload (linked for APP1) -> full package on external W25Q slot,
 * then OTA_Flag (OTA_DOWNLOADED). System reset hands off to bootloader.
 */
#include "app_ota_update.h"

#include "cmsis_os2.h"
#include "event_conf.h"
#include "fatfs.h"
#include "main.h"
#include "ota_download.h"
#include "ota_error_codes.h"
#include "ota_flag.h"
#include "ota_fw_slot.h"
#include "ota_image.h"
#include "ota_info.h"
#include "system_storge.h"

#include "stm32h7xx_hal_cortex.h"

#include <stdio.h>
#include <string.h>

#define OTA_APP_FILE_PATH    "0:/app.bin"

extern osEventFlagsId_t ota_download_Event;

static OTA_Update_Result_t OTA_MergeBaselineFlag(OTA_Flag_t *out);
static OTA_Update_Result_t OTA_CheckFlagAllowsNewStage(void);
static OTA_Update_Result_t OTA_StageUsbImageToExternalFlash(const char *file_path);
static void OTA_DoUpdate(void);

void OTA_Update_Task(void *argument)
{
    uint32_t flags;

    (void)argument;

    for (;;) {
        flags = osEventFlagsWait(ota_download_Event,
                                 OTA_MSG_EVT_START | OTA_MSG_EVT_CANCEL,
                                 osFlagsWaitAny,
                                 osWaitForever);

        if ((flags & osFlagsError) != 0U) {
            continue;
        }

        if ((flags & OTA_MSG_EVT_START) != 0U) {
            OTA_DoUpdate();
        }

        if ((flags & OTA_MSG_EVT_CANCEL) != 0U) {
            printf("OTA cancel\r\n");
        }
    }
}

static void OTA_DoUpdate(void)
{
    OTA_Update_Result_t ret;

    printf("OTA stage start\r\n");

    ret = OTA_StageUsbImageToExternalFlash(OTA_APP_FILE_PATH);
    if (ret != OTA_OK) {
        printf("OTA stage failed, ret=%d\r\n", (int)ret);
        return;
    }

    printf("OTA staged, resetting for bootloader install\r\n");
    HAL_NVIC_SystemReset();
}

static OTA_Update_Result_t OTA_MergeBaselineFlag(OTA_Flag_t *out)
{
    OTA_Update_Result_t r;

    if (out == NULL) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    r = OTA_GetValidFlag(out);
    if (r == OTA_OK) {
        return OTA_OK;
    }

    if (r == OTA_UPDATE_READFLAG_ERROR) {
        memset(out, 0, sizeof(*out));
        out->magic = OTA_FLAG_MAGIC;
        out->version = 0U;
        out->seq = 1U;
        out->ota_state = OTA_IDLE;
        out->committed_slot = OTA_FW_SLOT_A;
        out->staging_slot = OTA_FW_SLOT_NONE;
        out->new_crc32 = 0U;
        out->boot_count = 0U;
        out->last_result = 0U;
        out->flag_crc32 = 0U;
        return OTA_OK;
    }

    return r;
}

static OTA_Update_Result_t OTA_CheckFlagAllowsNewStage(void)
{
    OTA_Flag_t fl;
    OTA_Update_Result_t r;

    r = OTA_GetValidFlag(&fl);
    if (r == OTA_UPDATE_READFLAG_ERROR) {
        return OTA_OK;
    }
    if (r != OTA_OK) {
        return r;
    }

    switch (fl.ota_state) {
    case OTA_IDLE:
    case OTA_FAILED:
        return OTA_OK;
    default:
        printf("OTA busy: ota_state=0x%08lX, deny new stage\r\n",
               (unsigned long)fl.ota_state);
        return OTA_UPDATE_FLAG_ERROR;
    }
}

static OTA_Update_Result_t OTA_StageUsbImageToExternalFlash(const char *file_path)
{
    FIL file;
    FRESULT fr;
    uint32_t file_size;
    Image_Info_t hdr;
    uint8_t prefix8[8];
    uint32_t payload_off = (uint32_t)sizeof(Image_Info_t);
    uint32_t max_payload;
    OTA_Update_Result_t ret;
    OTA_Flag_t nf;
    uint32_t staging_slot;
    uint32_t hdr_sz;

    max_payload = OTA_NEW_FW_SIZE;
    if (APP1_FLASH_SIZE < max_payload) {
        max_payload = APP1_FLASH_SIZE;
    }

    if (file_path == NULL) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    ret = OTA_CheckFlagAllowsNewStage();
    if (ret != OTA_OK) {
        return ret;
    }

    ret = OTA_MergeBaselineFlag(&nf);
    if (ret != OTA_OK) {
        return ret;
    }

    fr = f_open(&file, file_path, FA_READ);
    if (fr != FR_OK) {
        printf("OTA open %s failed, fr=%d\r\n", file_path, (int)fr);
        return OTA_UPDATE_OPEN_ERROR;
    }

    file_size = (uint32_t)f_size(&file);
    hdr_sz = (uint32_t)sizeof(Image_Info_t);

    if ((file_size <= hdr_sz) || (file_size > (hdr_sz + max_payload))) {
        printf("OTA invalid file size=%lu\r\n", (unsigned long)file_size);
        f_close(&file);
        return OTA_UPDATE_SIZE_ERROR;
    }

    ret = OTA_Image_LoadHeaderFromFile(&file, &hdr);
    if (ret != OTA_OK) {
        f_close(&file);
        return ret;
    }

    ret = OTA_Image_Validate(&hdr, file_size, max_payload);
    if (ret != OTA_OK) {
        f_close(&file);
        return ret;
    }

    if (hdr.image_size < 8U) {
        f_close(&file);
        return OTA_UPDATE_IMAGE_ERROR;
    }

    ret = OTA_Image_LoadPayloadPrefixFromFile(&file, payload_off, prefix8);
    if (ret != OTA_OK) {
        f_close(&file);
        return ret;
    }

    ret = OTA_Image_ValidatePayloadForSlot(prefix8,
                                           APP1_FLASH_ADDR,
                                           APP1_FLASH_SIZE);
    if (ret != OTA_OK) {
        f_close(&file);
        return ret;
    }

    staging_slot = OTA_Fw_PickStagingSlot(nf.committed_slot);
    if (staging_slot == OTA_FW_SLOT_NONE) {
        f_close(&file);
        return OTA_UPDATE_PARAM_ERROR;
    }

    ret = OTA_StageFullFirmwarePkgFromFile(&file, staging_slot);
    if (ret != OTA_OK) {
        f_close(&file);
        return ret;
    }

    f_close(&file);

    nf.seq++;
    nf.ota_state = OTA_DOWNLOADED;
    nf.staging_slot = staging_slot;
    nf.new_crc32 = hdr.image_crc32;
    nf.last_result = 0U;

    ret = OTA_FlagWriteMerged(&nf);
    if (ret != OTA_OK) {
        return ret;
    }

    printf("OTA flag updated: DOWNLOADED, staging_slot=%lu\r\n",
           (unsigned long)staging_slot);

    return OTA_OK;
}
