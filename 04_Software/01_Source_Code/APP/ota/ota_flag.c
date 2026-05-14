#include <string.h>

#include "flash_w25q256.h"
#include "system_storge.h"

#include "ota_error_codes.h"
//#include "crc32.h"
#include "ota_download.h"
#include "ota_fw_slot.h"

#include "ota_flag.h"
#include "ota_info.h"

typedef struct {
    OTA_Flag_t         flag;
    uint32_t           slot;
    uint32_t           valid;
    uint32_t           read_ok;
    uint32_t           magic_ok;
    //uint32_t           crc32_ok;
} Flag_Info_t;

static void GET_VALID_FLAG(Flag_Info_t *flag_info);
static void READ_FLAG(Flag_Info_t *flag_info);
static void CHECK_FLAG_MAGIC(Flag_Info_t *flag_info);
//static void CALC_FLAG_CRC32(Flag_Info_t *flag_info);

/* 获取有效flag区 */
OTA_Update_Result_t OTA_GetValidFlag(OTA_Flag_t *flag)
{
    Flag_Info_t flag_info1 = {0};
    Flag_Info_t flag_info2 = {0};

    if (flag == NULL) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    if (W25Q256_Init() != W25Q256_OK) {
        return OTA_UPDATE_EXT_FLASH_ERROR;
    }

    flag_info1.slot = OTA_FLAG1_ADDR;
    flag_info2.slot = OTA_FLAG2_ADDR;

    GET_VALID_FLAG(&flag_info1);
    GET_VALID_FLAG(&flag_info2);

    if((0 == flag_info1.valid) && (0 == flag_info2.valid))
    {
        return OTA_UPDATE_READFLAG_ERROR;
    }
    else if((1 == flag_info1.valid) && (0 == flag_info2.valid))
    {
        memcpy(flag, &flag_info1.flag, sizeof(OTA_Flag_t));
    }
    else if((0 == flag_info1.valid) && (1 == flag_info2.valid))
    {
        memcpy(flag, &flag_info2.flag, sizeof(OTA_Flag_t));
    }
    else
    {
        memcpy(flag, 
               flag_info1.flag.seq > flag_info2.flag.seq ? &flag_info1.flag : &flag_info2.flag, 
               sizeof(OTA_Flag_t));
    }
    return OTA_OK;
}

static void GET_VALID_FLAG(Flag_Info_t *flag_info)
{
    READ_FLAG(flag_info);
    if(flag_info->read_ok == 0)
    {
        return;
    }
    CHECK_FLAG_MAGIC(flag_info);
    if(flag_info->magic_ok == 0)
    {
        return;
    }
    //CALC_FLAG_CRC32(flag_info);
    //if(flag_info->crc32_ok == 0)
    //{
    //    return;
    //}
    flag_info->valid = 1;
    return;
}

//能否读取到flag区
static void READ_FLAG(Flag_Info_t *flag_info)
{
    if(W25Q256_OK != W25Q256_Read(flag_info->slot, 
                                   (uint8_t *)&flag_info->flag, 
                                   sizeof(OTA_Flag_t)))
    {
        flag_info->read_ok = 0;
        return;
    }
    flag_info->read_ok = 1;
    return;
}

static void CHECK_FLAG_MAGIC(Flag_Info_t *flag_info)
{
    if(OTA_FLAG_MAGIC != flag_info->flag.magic)
    {
        flag_info->magic_ok = 0;
        return;
    }
    flag_info->magic_ok = 1;
    return;
}

static uint32_t pick_winner_physical_addr(const Flag_Info_t *f1,
                                          const Flag_Info_t *f2)
{
    if ((f1->valid != 1U) && (f2->valid != 1U)) {
        return 0U;
    }

    if ((f1->valid == 1U) && (f2->valid != 1U)) {
        return f1->slot;
    }

    if ((f1->valid != 1U) && (f2->valid == 1U)) {
        return f2->slot;
    }

    if (f1->flag.seq >= f2->flag.seq) {
        return f1->slot;
    }

    return f2->slot;
}

static uint32_t inactive_flag_physical_addr(uint32_t winner_physical)
{
    if (winner_physical == OTA_FLAG1_ADDR) {
        return OTA_FLAG2_ADDR;
    }

    return OTA_FLAG1_ADDR;
}

OTA_Update_Result_t OTA_FlagWriteMerged(const OTA_Flag_t *flag)
{
    Flag_Info_t flag_info1 = {0};
    Flag_Info_t flag_info2 = {0};
    uint32_t winner;
    uint32_t inactive;
    OTA_Flag_t wb;

    if (flag == NULL) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    if (flag->magic != OTA_FLAG_MAGIC) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    if (W25Q256_Init() != W25Q256_OK) {
        return OTA_UPDATE_EXT_FLASH_ERROR;
    }

    flag_info1.slot = OTA_FLAG1_ADDR;
    flag_info2.slot = OTA_FLAG2_ADDR;
    GET_VALID_FLAG(&flag_info1);
    GET_VALID_FLAG(&flag_info2);

    winner = pick_winner_physical_addr(&flag_info1, &flag_info2);
    inactive = inactive_flag_physical_addr(
        (winner == 0U) ? OTA_FLAG2_ADDR : winner);

    memcpy(&wb, flag, sizeof(wb));
    wb.flag_crc32 = 0U;

    if (OTA_EraseW25Aligned(inactive, OTA_FLAG_SIZE) != OTA_OK) {
        return OTA_UPDATE_EXT_FLASH_ERROR;
    }

    if (W25Q256_Write(inactive,
                      (uint8_t *)&wb,
                      sizeof(wb)) != W25Q256_OK) {
        return OTA_UPDATE_WRITE_ERROR;
    }

    {
        OTA_Flag_t verify;
        if (W25Q256_Read(inactive,
                         (uint8_t *)&verify,
                         sizeof(verify)) != W25Q256_OK) {
            return OTA_UPDATE_VERIFY_ERROR;
        }
        if (memcmp(&verify, &wb, sizeof(verify)) != 0) {
            return OTA_UPDATE_VERIFY_ERROR;
        }
    }

    return OTA_OK;
}

OTA_Update_Result_t OTA_AppConfirmTrialSuccess(void)
{
    OTA_Flag_t fl;
    OTA_Update_Result_t r;

    r = OTA_GetValidFlag(&fl);
    if (r != OTA_OK) {
        return r;
    }

    if (fl.ota_state != OTA_INSTALLED) {
        return OTA_OK;
    }

    fl.committed_slot = fl.staging_slot;
    fl.staging_slot = OTA_FW_SLOT_NONE;
    fl.boot_count = 0U;
    fl.ota_state = OTA_IDLE;
    fl.seq++;

    return OTA_FlagWriteMerged(&fl);
}
