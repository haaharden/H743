#include "flash_if.h"
#include <string.h>
#include <stdio.h>

static uint8_t FlashIf_IsAddrInApp2(uint32_t addr, uint32_t len)
{
    if (len == 0U)
    {
        return 0U;
    }

    if (addr < APP2_FLASH_ADDR)
    {
        return 0U;
    }

    if ((addr + len) > APP2_FLASH_END_ADDR)
    {
        return 0U;
    }

    if ((addr + len) < addr)
    {
        return 0U;
    }

    return 1U;
}

FlashIf_Result_t FlashIf_EraseApp2(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error = 0U;
    HAL_StatusTypeDef status;

    /*
     * 你的 APP2:
     * 0x08100000 ~ 0x081DFFFF
     *
     * STM32H743 2MB 常见布局：
     * Bank2 起始地址 0x08100000
     * 每个 sector 128KB
     * APP2 大小 896KB = 7 个 sector
     *
     * 所以擦 Bank2 Sector 0 ~ Sector 6
     */
    HAL_FLASH_Unlock();

    memset(&erase, 0, sizeof(erase));

    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.Banks        = FLASH_BANK_2;
    erase.Sector       = FLASH_SECTOR_0;
    erase.NbSectors    = 7U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    status = HAL_FLASHEx_Erase(&erase, &sector_error);

    HAL_FLASH_Lock();

    if (status != HAL_OK)
    {
        printf("Flash erase APP2 failed, sector_error = 0x%08lX\r\n",
               (unsigned long)sector_error);
        return FLASH_IF_ERASE_ERROR;
    }

    printf("Flash erase APP2 OK\r\n");

    return FLASH_IF_OK;
}

FlashIf_Result_t FlashIf_Write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t offset = 0U;
    uint32_t write_len;
    uint32_t flash_word[8];
    HAL_StatusTypeDef status;

    if (data == NULL)
    {
        return FLASH_IF_SIZE_ERROR;
    }

    if (FlashIf_IsAddrInApp2(addr, len) == 0U)
    {
        return FLASH_IF_ADDR_ERROR;
    }

    /*
     * STM32H7 Flash Word 写入地址要求 32 字节对齐。
     */
    if ((addr % FLASH_WORD_SIZE) != 0U)
    {
        return FLASH_IF_ADDR_ERROR;
    }

    HAL_FLASH_Unlock();

    while (offset < len)
    {
        write_len = len - offset;

        if (write_len > FLASH_WORD_SIZE)
        {
            write_len = FLASH_WORD_SIZE;
        }

        /*
         * 最后一包如果不足 32 字节，用 0xFF 补齐。
         * 0xFF 是 Flash 擦除后的状态。
         */
        memset(flash_word, 0xFF, sizeof(flash_word));
        memcpy((uint8_t *)flash_word, &data[offset], write_len);

        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                                   addr + offset,
                                   (uint32_t)flash_word);

        if (status != HAL_OK)
        {
            HAL_FLASH_Lock();

            printf("Flash write failed at 0x%08lX, err = 0x%08lX\r\n",
                   (unsigned long)(addr + offset),
                   (unsigned long)HAL_FLASH_GetError());

            return FLASH_IF_WRITE_ERROR;
        }

        offset += FLASH_WORD_SIZE;
    }

    HAL_FLASH_Lock();

    return FLASH_IF_OK;
}

FlashIf_Result_t FlashIf_Verify(uint32_t addr, const uint8_t *data, uint32_t len)
{
    const uint8_t *flash_ptr;

    if (data == NULL)
    {
        return FLASH_IF_SIZE_ERROR;
    }

    if (FlashIf_IsAddrInApp2(addr, len) == 0U)
    {
        return FLASH_IF_ADDR_ERROR;
    }

    flash_ptr = (const uint8_t *)addr;

    for (uint32_t i = 0U; i < len; i++)
    {
        if (flash_ptr[i] != data[i])
        {
            printf("Flash verify failed at 0x%08lX, flash=0x%02X, data=0x%02X\r\n",
                   (unsigned long)(addr + i),
                   flash_ptr[i],
                   data[i]);

            return FLASH_IF_VERIFY_ERROR;
        }
    }

    return FLASH_IF_OK;
}