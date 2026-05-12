/*
 * OTA task:
 * 1. Read 0:/app.bin from the USB disk.
 * 2. Stage it into external W25Q256 flash at OTA_NEW_FW_ADDR.
 * 3. Write BootInfo_t at BOOT_INFO_ADDR so bootloader installs it.
 */
#include "ota_update.h"

#include "cmsis_os2.h"
#include "event_conf.h"
#include "fatfs.h"
#include "flash_w25q256.h"
#include "main.h"
#include "system_storge.h"

#include <stdio.h>
#include <string.h>

#define OTA_APP_FILE_PATH       "0:/app.bin"
#define OTA_IO_BUF_SIZE         4096U
#define OTA_FLASH_WORD_SIZE     32U

#define OTA_BOOT_INFO_BANK      FLASH_BANK_2
#define OTA_BOOT_INFO_SECTOR    FLASH_SECTOR_7

#define OTA_SRAM1_BASE          0x20000000UL
#define OTA_SRAM_END            0x38000000UL

typedef enum
{
    OTA_UPDATE_OK = 0,
    OTA_UPDATE_PARAM_ERROR = -1,
    OTA_UPDATE_OPEN_ERROR = -2,
    OTA_UPDATE_SIZE_ERROR = -3,
    OTA_UPDATE_IMAGE_ERROR = -4,
    OTA_UPDATE_EXT_FLASH_ERROR = -5,
    OTA_UPDATE_READ_ERROR = -6,
    OTA_UPDATE_WRITE_ERROR = -7,
    OTA_UPDATE_VERIFY_ERROR = -8,
    OTA_UPDATE_BOOT_INFO_ERROR = -9
} OTA_UpdateResult_t;

extern osEventFlagsId_t otaEventHandle;

static uint8_t s_ota_read_buf[OTA_IO_BUF_SIZE];
static uint8_t s_ota_verify_buf[OTA_IO_BUF_SIZE];

static OTA_UpdateResult_t OTA_StageFileToExternalFlash(const char *file_path);
static OTA_UpdateResult_t OTA_CheckImageHeader(FIL *file, uint32_t file_size);
static OTA_UpdateResult_t OTA_EraseExternalFlash(uint32_t address, uint32_t length);
static OTA_UpdateResult_t OTA_WriteBootInfo(uint32_t image_size);

static void OTA_DoUpdate(void)
{
    OTA_UpdateResult_t ret;

    printf("OTA stage start\r\n");

    ret = OTA_StageFileToExternalFlash(OTA_APP_FILE_PATH);
    if (ret != OTA_UPDATE_OK)
    {
        printf("OTA stage failed, ret=%d\r\n", ret);
        return;
    }

    printf("OTA stage success, reboot to let bootloader install\r\n");
}

void OTA_Update_Task(void *argument)
{
    uint32_t flags;

    (void)argument;

    for (;;)
    {
        flags = osEventFlagsWait(otaEventHandle,
                                 OTA_MSG_EVT_START | OTA_MSG_EVT_CANCEL,
                                 osFlagsWaitAny,
                                 osWaitForever);

        if ((flags & osFlagsError) != 0U)
        {
            continue;
        }

        if ((flags & OTA_MSG_EVT_START) != 0U)
        {
            OTA_DoUpdate();
        }

        if ((flags & OTA_MSG_EVT_CANCEL) != 0U)
        {
            printf("OTA cancel\r\n");
        }
    }
}

static OTA_UpdateResult_t OTA_StageFileToExternalFlash(const char *file_path)
{
    FIL file;
    FRESULT fr;
    UINT read_len;
    uint32_t file_size;
    uint32_t offset = 0U;
    OTA_UpdateResult_t ret;

    if (file_path == NULL)
    {
        return OTA_UPDATE_PARAM_ERROR;
    }

    fr = f_open(&file, file_path, FA_READ);
    if (fr != FR_OK)
    {
        printf("OTA open %s failed, fr=%d\r\n", file_path, fr);
        return OTA_UPDATE_OPEN_ERROR;
    }

    file_size = (uint32_t)f_size(&file);
    if ((file_size == 0U) ||
         (file_size > OTA_NEW_FW_SIZE) ||
         (file_size > OTA_TARGET_FLASH_SIZE))
    {
         printf("OTA invalid file size=%lu, ext_max=%lu, target_max=%lu\r\n",
               (unsigned long)file_size,
               (unsigned long)OTA_NEW_FW_SIZE,
             (unsigned long)OTA_TARGET_FLASH_SIZE);
        f_close(&file);
        return OTA_UPDATE_SIZE_ERROR;
    }

    ret = OTA_CheckImageHeader(&file, file_size);
    if (ret != OTA_UPDATE_OK)
    {
        f_close(&file);
        return ret;
    }

    ret = OTA_EraseExternalFlash(OTA_NEW_FW_ADDR, file_size);
    if (ret != OTA_UPDATE_OK)
    {
        f_close(&file);
        return ret;
    }

    fr = f_lseek(&file, 0U);
    if (fr != FR_OK)
    {
        f_close(&file);
        return OTA_UPDATE_READ_ERROR;
    }

    while (offset < file_size)
    {
        fr = f_read(&file, s_ota_read_buf, OTA_IO_BUF_SIZE, &read_len);
        if (fr != FR_OK)
        {
            printf("OTA read failed, fr=%d, offset=%lu\r\n",
                   fr,
                   (unsigned long)offset);
            f_close(&file);
            return OTA_UPDATE_READ_ERROR;
        }

        if (read_len == 0U)
        {
            break;
        }

        if (W25Q256_Write(OTA_NEW_FW_ADDR + offset,
                          s_ota_read_buf,
                          read_len) != W25Q256_OK)
        {
            printf("OTA external flash write failed, offset=%lu\r\n",
                   (unsigned long)offset);
            f_close(&file);
            return OTA_UPDATE_WRITE_ERROR;
        }

        if (W25Q256_Read(OTA_NEW_FW_ADDR + offset,
                         s_ota_verify_buf,
                         read_len) != W25Q256_OK)
        {
            printf("OTA external flash readback failed, offset=%lu\r\n",
                   (unsigned long)offset);
            f_close(&file);
            return OTA_UPDATE_VERIFY_ERROR;
        }

        if (memcmp(s_ota_read_buf, s_ota_verify_buf, read_len) != 0)
        {
            printf("OTA external flash verify mismatch, offset=%lu\r\n",
                   (unsigned long)offset);
            f_close(&file);
            return OTA_UPDATE_VERIFY_ERROR;
        }

        offset += read_len;

        printf("OTA stage progress: %lu/%lu\r\n",
               (unsigned long)offset,
               (unsigned long)file_size);
    }

    f_close(&file);

    if (offset != file_size)
    {
        printf("OTA size mismatch, staged=%lu, file=%lu\r\n",
               (unsigned long)offset,
               (unsigned long)file_size);
        return OTA_UPDATE_SIZE_ERROR;
    }

    ret = OTA_WriteBootInfo(file_size);
    if (ret != OTA_UPDATE_OK)
    {
        return ret;
    }

    printf("OTA staged to external flash, addr=0x%08lX, size=%lu\r\n",
           (unsigned long)OTA_NEW_FW_ADDR,
           (unsigned long)file_size);

    return OTA_UPDATE_OK;
}

static OTA_UpdateResult_t OTA_CheckImageHeader(FIL *file, uint32_t file_size)
{
    FRESULT fr;
    UINT read_len;
    uint32_t initial_sp;
    uint32_t reset_handler;

    if ((file == NULL) || (file_size < 8U))
    {
        return OTA_UPDATE_IMAGE_ERROR;
    }

    fr = f_lseek(file, 0U);
    if (fr != FR_OK)
    {
        return OTA_UPDATE_READ_ERROR;
    }

    fr = f_read(file, s_ota_read_buf, 8U, &read_len);
    if ((fr != FR_OK) || (read_len != 8U))
    {
        return OTA_UPDATE_READ_ERROR;
    }

    memcpy(&initial_sp, &s_ota_read_buf[0], sizeof(initial_sp));
    memcpy(&reset_handler, &s_ota_read_buf[4], sizeof(reset_handler));

    if ((initial_sp < OTA_SRAM1_BASE) || (initial_sp >= OTA_SRAM_END))
    {
        printf("OTA invalid initial SP: 0x%08lX\r\n",
               (unsigned long)initial_sp);
        return OTA_UPDATE_IMAGE_ERROR;
    }

    if (((reset_handler & 1U) == 0U) ||
        ((reset_handler & ~1UL) < OTA_TARGET_FLASH_ADDR) ||
        ((reset_handler & ~1UL) >= OTA_TARGET_FLASH_END_ADDR))
    {
        printf("OTA invalid reset handler: 0x%08lX\r\n",
               (unsigned long)reset_handler);
        return OTA_UPDATE_IMAGE_ERROR;
    }

    return OTA_UPDATE_OK;
}

static OTA_UpdateResult_t OTA_EraseExternalFlash(uint32_t address, uint32_t length)
{
    uint32_t erase_addr;
    uint32_t erase_end;

    if ((length == 0U) ||
        (address < OTA_NEW_FW_ADDR) ||
        ((address + length) > (OTA_NEW_FW_ADDR + OTA_NEW_FW_SIZE)) ||
        ((address + length) < address))
    {
        return OTA_UPDATE_PARAM_ERROR;
    }

    if (W25Q256_Init() != W25Q256_OK)
    {
        return OTA_UPDATE_EXT_FLASH_ERROR;
    }

    erase_addr = address - (address % W25Q256_ERASE_SIZE);
    erase_end = address + length;
    erase_end = (erase_end + W25Q256_ERASE_SIZE - 1U) & ~(W25Q256_ERASE_SIZE - 1U);

    while (erase_addr < erase_end)
    {
        if (W25Q256_EraseSector(erase_addr) != W25Q256_OK)
        {
            printf("OTA erase external flash failed, addr=0x%08lX\r\n",
                   (unsigned long)erase_addr);
            return OTA_UPDATE_EXT_FLASH_ERROR;
        }

        erase_addr += W25Q256_ERASE_SIZE;
    }

    return OTA_UPDATE_OK;
}

// 这里直接写 BootInfo_t，后续可以改成先写一个 OTA_FLAG，Bootloader 看到这个 flag 后再读 BootInfo_t 来安装。
static OTA_UpdateResult_t OTA_WriteBootInfo(uint32_t image_size)
{
    BootInfo_t boot_info;
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error = 0U;
    uint32_t flash_word[OTA_FLASH_WORD_SIZE / sizeof(uint32_t)];
    HAL_StatusTypeDef status;

    memset(&boot_info, 0, sizeof(boot_info));
    boot_info.magic = BOOT_INFO_MAGIC;
    boot_info.active_slot = OTA_RUNNING_SLOT;
    boot_info.target_slot = OTA_TARGET_SLOT;
    boot_info.update_state = BOOT_UPDATE_STAGED;
    boot_info.image_size = image_size;
    boot_info.image_ext_addr = OTA_NEW_FW_ADDR;
    boot_info.last_result = 0U;

    memset(&erase, 0, sizeof(erase));
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks = OTA_BOOT_INFO_BANK;
    erase.Sector = OTA_BOOT_INFO_SECTOR;
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_FLASH_Unlock();

    status = HAL_FLASHEx_Erase(&erase, &sector_error);
    if (status != HAL_OK)
    {
        HAL_FLASH_Lock();
        printf("OTA erase boot info failed, sector_error=0x%08lX\r\n",
               (unsigned long)sector_error);
        return OTA_UPDATE_BOOT_INFO_ERROR;
    }

    memset(flash_word, 0xFF, sizeof(flash_word));
    memcpy(flash_word, &boot_info, sizeof(boot_info));

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                               BOOT_INFO_ADDR,
                               (uint32_t)flash_word);
    HAL_FLASH_Lock();

    if (status != HAL_OK)
    {
        printf("OTA write boot info failed, err=0x%08lX\r\n",
               (unsigned long)HAL_FLASH_GetError());
        return OTA_UPDATE_BOOT_INFO_ERROR;
    }

    if (memcmp((const void *)BOOT_INFO_ADDR, &boot_info, sizeof(boot_info)) != 0)
    {
        printf("OTA boot info verify failed\r\n");
        return OTA_UPDATE_BOOT_INFO_ERROR;
    }

        printf("OTA boot info staged, target=%s, size=%lu\r\n",
            OTA_TARGET_NAME,
           (unsigned long)image_size);

    return OTA_UPDATE_OK;
}
