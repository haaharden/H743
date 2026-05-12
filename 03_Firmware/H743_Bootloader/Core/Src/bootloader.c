#include "bootloader.h"
#include "flash_w25q256.h"
#include "quadspi.h"
#include "usart.h"

#include <stdio.h>
#include <string.h>

static uint8_t s_install_buf[BOOT_INSTALL_CHUNK_SIZE];

static uint8_t Bootloader_ReadBootInfo(BootInfo_t *boot_info);
static uint8_t Bootloader_WriteBootInfo(const BootInfo_t *boot_info);
static uint8_t Bootloader_InstallPendingImage(BootInfo_t *boot_info);
static uint8_t Bootloader_ValidateStagedImage(const BootInfo_t *boot_info);
static uint8_t Bootloader_EraseSlot(uint32_t slot);
static uint8_t Bootloader_WriteImageToSlot(uint32_t slot, uint32_t ext_addr, uint32_t image_size);
static uint32_t Bootloader_GetSlotAddr(uint32_t slot);
static uint32_t Bootloader_GetSlotSize(uint32_t slot);
static uint32_t Bootloader_GetSlotBank(uint32_t slot);
static uint32_t Bootloader_GetSlotFirstSector(uint32_t slot);
static uint32_t Bootloader_GetSlotSectorCount(uint32_t slot);
static uint8_t Bootloader_IsValidBootInfo(const BootInfo_t *boot_info);
static uint32_t Bootloader_SelectBootSlot(const BootInfo_t *boot_info);
static uint8_t Bootloader_IsValidSlot(uint32_t slot);
static uint8_t Bootloader_IsValidResetHandler(uint32_t pc,
                                               uint32_t app_addr,
                                               uint32_t app_size);
static uint8_t Bootloader_IsValidStack(uint32_t sp);
static uint8_t Bootloader_IsValidSlot(uint32_t slot)
{
    return ((slot == BOOT_SLOT_APP1) || (slot == BOOT_SLOT_APP2)) ? 1U : 0U;
}

static uint8_t Bootloader_IsValidBootInfo(const BootInfo_t *boot_info)
{
    if (boot_info == NULL)
    {
        return 0U;
    }

    if (boot_info->magic != BOOT_INFO_MAGIC)
    {
        return 0U;
    }

    if ((boot_info->active_slot != BOOT_SLOT_NONE) &&
        (Bootloader_IsValidSlot(boot_info->active_slot) == 0U))
    {
        return 0U;
    }

    if ((boot_info->target_slot != BOOT_SLOT_NONE) &&
        (Bootloader_IsValidSlot(boot_info->target_slot) == 0U))
    {
        return 0U;
    }

    if (boot_info->update_state > BOOT_UPDATE_FAIL)
    {
        return 0U;
    }

    return 1U;
}

static uint8_t Bootloader_ReadBootInfo(BootInfo_t *boot_info)
{
    if (boot_info == NULL)
    {
        return 0U;
    }

    memset(boot_info, 0, sizeof(*boot_info));
    memcpy(boot_info, (const void *)BOOT_INFO_ADDR, sizeof(*boot_info));

    if (Bootloader_IsValidBootInfo(boot_info) == 0U)
    {
        memset(boot_info, 0, sizeof(*boot_info));
        boot_info->magic = BOOT_INFO_MAGIC;
        boot_info->active_slot = BOOT_SLOT_APP1;
        boot_info->update_state = BOOT_UPDATE_IDLE;
        return 0U;
    }

    if (boot_info->active_slot == BOOT_SLOT_NONE)
    {
        boot_info->active_slot = BOOT_SLOT_APP1;
    }

    return 1U;
}

static uint8_t Bootloader_WriteBootInfo(const BootInfo_t *boot_info)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error;
    uint32_t flash_word[BOOT_FLASH_WORD_SIZE / sizeof(uint32_t)];
    HAL_StatusTypeDef status;

    if (boot_info == NULL)
    {
        return 0U;
    }

    memset(&erase, 0, sizeof(erase));
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks = BOOT_INFO_BANK;
    erase.Sector = BOOT_INFO_SECTOR;
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_FLASH_Unlock();

    sector_error = 0U;
    status = HAL_FLASHEx_Erase(&erase, &sector_error);
    if (status != HAL_OK)
    {
        HAL_FLASH_Lock();
        printf("Boot info erase failed, sector_error=0x%08lX\r\n",
               (unsigned long)sector_error);
        return 0U;
    }

    memset(flash_word, 0xFF, sizeof(flash_word));
    memcpy(flash_word, boot_info, sizeof(*boot_info));

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                               BOOT_INFO_ADDR,
                               (uint32_t)flash_word);
    HAL_FLASH_Lock();

    if (status != HAL_OK)
    {
        printf("Boot info write failed, err=0x%08lX\r\n",
               (unsigned long)HAL_FLASH_GetError());
        return 0U;
    }

    if (memcmp((const void *)BOOT_INFO_ADDR, boot_info, sizeof(*boot_info)) != 0)
    {
        printf("Boot info verify failed\r\n");
        return 0U;
    }

    return 1U;
}

static uint32_t Bootloader_GetSlotAddr(uint32_t slot)
{
    if (slot == BOOT_SLOT_APP1)
    {
        return BOOT_APP1_ADDR;
    }

    if (slot == BOOT_SLOT_APP2)
    {
        return BOOT_APP2_ADDR;
    }

    return 0U;
}

static uint32_t Bootloader_GetSlotSize(uint32_t slot)
{
    if (slot == BOOT_SLOT_APP1)
    {
        return BOOT_APP1_SIZE;
    }

    if (slot == BOOT_SLOT_APP2)
    {
        return BOOT_APP2_SIZE;
    }

    return 0U;
}

static uint32_t Bootloader_GetSlotBank(uint32_t slot)
{
    if (slot == BOOT_SLOT_APP1)
    {
        return BOOT_APP1_BANK;
    }

    if (slot == BOOT_SLOT_APP2)
    {
        return BOOT_APP2_BANK;
    }

    return 0U;
}

static uint32_t Bootloader_GetSlotFirstSector(uint32_t slot)
{
    if (slot == BOOT_SLOT_APP1)
    {
        return BOOT_APP1_FIRST_SECTOR;
    }

    if (slot == BOOT_SLOT_APP2)
    {
        return BOOT_APP2_FIRST_SECTOR;
    }

    return 0U;
}

static uint32_t Bootloader_GetSlotSectorCount(uint32_t slot)
{
    if (slot == BOOT_SLOT_APP1)
    {
        return BOOT_APP1_SECTOR_COUNT;
    }

    if (slot == BOOT_SLOT_APP2)
    {
        return BOOT_APP2_SECTOR_COUNT;
    }

    return 0U;
}

static uint8_t Bootloader_ValidateStagedImage(const BootInfo_t *boot_info)
{
    uint32_t image_header[2];
    uint32_t target_addr;
    uint32_t target_size;

    if (boot_info == NULL)
    {
        return 0U;
    }

    target_addr = Bootloader_GetSlotAddr(boot_info->target_slot);
    target_size = Bootloader_GetSlotSize(boot_info->target_slot);
    if ((target_addr == 0U) || (target_size == 0U))
    {
        return 0U;
    }

    if ((boot_info->image_size == 0U) ||
        (boot_info->image_size > target_size) ||
        (boot_info->image_size > BOOT_OTA_NEW_FW_SIZE))
    {
        printf("Invalid staged image size=%lu\r\n",
               (unsigned long)boot_info->image_size);
        return 0U;
    }

    if ((boot_info->image_ext_addr < BOOT_OTA_NEW_FW_ADDR) ||
        ((boot_info->image_ext_addr + boot_info->image_size) > BOOT_EXT_FLASH_SIZE) ||
        ((boot_info->image_ext_addr + boot_info->image_size) < boot_info->image_ext_addr))
    {
        printf("Invalid staged image ext addr=0x%08lX\r\n",
               (unsigned long)boot_info->image_ext_addr);
        return 0U;
    }

    if (W25Q256_Read(boot_info->image_ext_addr,
                     (uint8_t *)image_header,
                     sizeof(image_header)) != W25Q256_OK)
    {
        printf("Read staged image header failed\r\n");
        return 0U;
    }

    if (Bootloader_IsValidStack(image_header[0]) == 0U)
    {
        printf("Invalid staged MSP=0x%08lX\r\n",
               (unsigned long)image_header[0]);
        return 0U;
    }

    if (Bootloader_IsValidResetHandler(image_header[1], target_addr, target_size) == 0U)
    {
        printf("Invalid staged Reset=0x%08lX\r\n",
               (unsigned long)image_header[1]);
        return 0U;
    }

    return 1U;
}

static uint8_t Bootloader_EraseSlot(uint32_t slot)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error;
    HAL_StatusTypeDef status;

    memset(&erase, 0, sizeof(erase));
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks = Bootloader_GetSlotBank(slot);
    erase.Sector = Bootloader_GetSlotFirstSector(slot);
    erase.NbSectors = Bootloader_GetSlotSectorCount(slot);
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    if ((erase.Banks == 0U) || (erase.NbSectors == 0U))
    {
        return 0U;
    }

    HAL_FLASH_Unlock();
    sector_error = 0U;
    status = HAL_FLASHEx_Erase(&erase, &sector_error);
    HAL_FLASH_Lock();

    if (status != HAL_OK)
    {
        printf("Erase slot %lu failed, sector_error=0x%08lX\r\n",
               (unsigned long)slot,
               (unsigned long)sector_error);
        return 0U;
    }

    return 1U;
}

static uint8_t Bootloader_WriteImageToSlot(uint32_t slot, uint32_t ext_addr, uint32_t image_size)
{
    uint32_t slot_addr;
    uint32_t offset;
    uint32_t chunk_size;
    uint32_t program_offset;
    uint32_t write_len;
    uint32_t flash_word[BOOT_FLASH_WORD_SIZE / sizeof(uint32_t)];
    HAL_StatusTypeDef status;

    slot_addr = Bootloader_GetSlotAddr(slot);
    if ((slot_addr == 0U) || (image_size == 0U))
    {
        return 0U;
    }

    if (Bootloader_EraseSlot(slot) == 0U)
    {
        return 0U;
    }

    HAL_FLASH_Unlock();

    offset = 0U;
    while (offset < image_size)
    {
        chunk_size = image_size - offset;
        if (chunk_size > BOOT_INSTALL_CHUNK_SIZE)
        {
            chunk_size = BOOT_INSTALL_CHUNK_SIZE;
        }

        if (W25Q256_Read(ext_addr + offset, s_install_buf, chunk_size) != W25Q256_OK)
        {
            HAL_FLASH_Lock();
            printf("Read staged image failed, offset=%lu\r\n",
                   (unsigned long)offset);
            return 0U;
        }

        program_offset = 0U;
        while (program_offset < chunk_size)
        {
            write_len = chunk_size - program_offset;
            if (write_len > BOOT_FLASH_WORD_SIZE)
            {
                write_len = BOOT_FLASH_WORD_SIZE;
            }

            memset(flash_word, 0xFF, sizeof(flash_word));
            memcpy((uint8_t *)flash_word, &s_install_buf[program_offset], write_len);

            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                                       slot_addr + offset + program_offset,
                                       (uint32_t)flash_word);
            if (status != HAL_OK)
            {
                HAL_FLASH_Lock();
                printf("Program slot %lu failed at 0x%08lX, err=0x%08lX\r\n",
                       (unsigned long)slot,
                       (unsigned long)(slot_addr + offset + program_offset),
                       (unsigned long)HAL_FLASH_GetError());
                return 0U;
            }

            program_offset += BOOT_FLASH_WORD_SIZE;
        }

        if (memcmp((const void *)(slot_addr + offset), s_install_buf, chunk_size) != 0)
        {
            HAL_FLASH_Lock();
            printf("Verify slot %lu failed at offset=%lu\r\n",
                   (unsigned long)slot,
                   (unsigned long)offset);
            return 0U;
        }

        offset += chunk_size;
    }

    HAL_FLASH_Lock();
    return 1U;
}

static uint8_t Bootloader_InstallPendingImage(BootInfo_t *boot_info)
{
    if (boot_info == NULL)
    {
        return 0U;
    }

    if ((boot_info->update_state != BOOT_UPDATE_STAGED) &&
        (boot_info->update_state != BOOT_UPDATE_INSTALLING))
    {
        return 1U;
    }

    if (Bootloader_IsValidSlot(boot_info->target_slot) == 0U)
    {
        printf("Invalid target slot=%lu\r\n",
               (unsigned long)boot_info->target_slot);
        return 0U;
    }

    MX_QUADSPI_Init();
    if (W25Q256_Init() != W25Q256_OK)
    {
        printf("W25Q256 init failed\r\n");
        return 0U;
    }

    if (Bootloader_ValidateStagedImage(boot_info) == 0U)
    {
        return 0U;
    }

    boot_info->update_state = BOOT_UPDATE_INSTALLING;
    boot_info->last_result = 0U;
    if (Bootloader_WriteBootInfo(boot_info) == 0U)
    {
        return 0U;
    }

    printf("Install staged image to APP%lu\r\n",
           (unsigned long)boot_info->target_slot);

    if (Bootloader_WriteImageToSlot(boot_info->target_slot,
                                    boot_info->image_ext_addr,
                                    boot_info->image_size) == 0U)
    {
        boot_info->update_state = BOOT_UPDATE_FAIL;
        boot_info->last_result = BOOT_UPDATE_FAIL;
        (void)Bootloader_WriteBootInfo(boot_info);
        return 0U;
    }

    boot_info->active_slot = boot_info->target_slot;
    boot_info->target_slot = BOOT_SLOT_NONE;
    boot_info->update_state = BOOT_UPDATE_IDLE;
    boot_info->last_result = BOOT_UPDATE_DONE;
    boot_info->image_size = 0U;
    boot_info->image_ext_addr = 0U;

    if (Bootloader_WriteBootInfo(boot_info) == 0U)
    {
        return 0U;
    }

    printf("Install success, update flag cleared\r\n");
    return 1U;
}

static uint32_t Bootloader_SelectBootSlot(const BootInfo_t *boot_info)
{
    if ((boot_info != NULL) &&
        (Bootloader_IsValidSlot(boot_info->active_slot) != 0U) &&
        (Bootloader_IsAppValid(Bootloader_GetSlotAddr(boot_info->active_slot),
                               Bootloader_GetSlotSize(boot_info->active_slot)) != 0U))
    {
        return boot_info->active_slot;
    }

    if (Bootloader_IsAppValid(BOOT_APP1_ADDR, BOOT_APP1_SIZE) != 0U)
    {
        return BOOT_SLOT_APP1;
    }

    if (Bootloader_IsAppValid(BOOT_APP2_ADDR, BOOT_APP2_SIZE) != 0U)
    {
        return BOOT_SLOT_APP2;
    }

    return BOOT_SLOT_NONE;
}


static uint8_t Bootloader_IsValidStack(uint32_t sp)
{
    /*
     * Cortex-M 栈建议 8 字节对齐。
     */
    if ((sp & 0x7U) != 0U)
    {
        return 0U;
    }

    /*
     * 初始 MSP 通常等于某段 RAM 的末尾地址，
     * 所以这里 end 允许等于。
     */
    if ((sp > BOOT_DTCM_RAM_START) && (sp <= BOOT_DTCM_RAM_END))
    {
        return 1U;
    }

    if ((sp > BOOT_AXI_RAM_START) && (sp <= BOOT_AXI_RAM_END))
    {
        return 1U;
    }

    if ((sp > BOOT_SRAM123_START) && (sp <= BOOT_SRAM123_END))
    {
        return 1U;
    }

    if ((sp > BOOT_SRAM4_START) && (sp <= BOOT_SRAM4_END))
    {
        return 1U;
    }

    return 0U;
}

static uint8_t Bootloader_IsValidResetHandler(uint32_t pc,
                                               uint32_t app_addr,
                                               uint32_t app_size)
{
    uint32_t real_pc;

    /*
     * Cortex-M 的函数入口地址最低位必须是 1，表示 Thumb 状态。
     * Reset_Handler 向量里保存的地址通常是 0x0802xxxx | 1。
     */
    if ((pc & 0x1U) == 0U)
    {
        return 0U;
    }

    real_pc = pc & 0xFFFFFFFEUL;

    if ((real_pc >= app_addr) && (real_pc < (app_addr + app_size)))
    {
        return 1U;
    }

    return 0U;
}

uint8_t Bootloader_IsAppValid(uint32_t app_addr, uint32_t app_size)
{
    uint32_t app_sp;
    uint32_t app_pc;

    app_sp = *(volatile uint32_t *)(app_addr);
    app_pc = *(volatile uint32_t *)(app_addr + 4U);

    if (Bootloader_IsValidStack(app_sp) == 0U)
    {
        return 0U;
    }

    if (Bootloader_IsValidResetHandler(app_pc, app_addr, app_size) == 0U)
    {
        return 0U;
    }

    return 1U;
}

static void Bootloader_DeInitBeforeJump(void)
{
    /*
     * 注意：
     * 这里不要一开始就 __disable_irq()，
     * 因为 HAL_RCC_DeInit() 内部可能依赖 HAL tick 做超时判断。
     *
     * 当前 Bootloader 只用了 USART1，所以先反初始化 USART1。
     * 后面如果 Bootloader 用了 USB、FatFs、QSPI、DMA、TIM，
     * 也要在这里逐个关闭。
     */
    HAL_UART_DeInit(&huart1);
    HAL_QSPI_DeInit(&hqspi);

    /*
     * 恢复 RCC 到更接近复位后的状态。
     * 这样 APP 的 SystemClock_Config() 更干净。
     */
    HAL_RCC_DeInit();

    /*
     * 反初始化 HAL。
     */
    HAL_DeInit();

    /*
     * 从这里开始彻底关中断和清现场。
     */
    __disable_irq();

    /*
     * 停止 SysTick。
     */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    /*
     * 关闭并清除所有 NVIC 外设中断。
     * 用 ICTR 获取 NVIC 寄存器组数量，更通用。
     */
    {
        uint32_t group_count;
        uint32_t i;

        group_count = ((SCnSCB->ICTR & 0xFU) + 1U);

        for (i = 0U; i < group_count; i++)
        {
            NVIC->ICER[i] = 0xFFFFFFFFU;
            NVIC->ICPR[i] = 0xFFFFFFFFU;
        }
    }

    /*
     * 清掉中断屏蔽状态，尽量恢复成普通复位后的状态。
     */
    __set_BASEPRI(0U);
    __set_FAULTMASK(0U);

    /*
     * 如果 Bootloader 开了 MPU，跳 APP 前建议关掉。
     * APP 后续自己重新配置 MPU。
     */
    HAL_MPU_Disable();
}

void Bootloader_JumpToApp(uint32_t app_addr)
{
    uint32_t app_sp;
    uint32_t app_pc;
    void (*app_entry)(void);

    app_sp = *(volatile uint32_t *)(app_addr);
    app_pc = *(volatile uint32_t *)(app_addr + 4U);

    app_entry = (void (*)(void))app_pc;

    printf("Jump to APP\r\n");
    printf("APP addr : 0x%08lX\r\n", (unsigned long)app_addr);
    printf("APP MSP  : 0x%08lX\r\n", (unsigned long)app_sp);
    printf("APP Reset: 0x%08lX\r\n", (unsigned long)app_pc);

    /*
     * 给串口一点时间发完最后的日志。
     */
    HAL_Delay(20U);

    Bootloader_DeInitBeforeJump();

    /*
     * 设置 APP 向量表地址。
     */
    SCB->VTOR = app_addr;

    /*
     * 确保使用 MSP，而不是 PSP。
     */
    __set_CONTROL(0U);

    /*
     * 设置 APP 的主栈。
     */
    __set_MSP(app_sp);

    __DSB();
    __ISB();

    /*
     * 恢复全局中断使能状态。
     * 因为真实硬件复位后 PRIMASK 默认是 0。
     * 如果这里不打开，APP 可能一直收不到中断。
     */
    __enable_irq();

    /*
     * 跳到 APP 的 Reset_Handler。
     */
    app_entry();

    /*
     * 正常不会回来。
     */
    while (1)
    {
    }
}

void Bootloader_Run(void)
{
    BootInfo_t boot_info;
    uint32_t boot_slot;

    printf("\r\n");
    printf("================================\r\n");
    printf("STM32H743 Bootloader start\r\n");
    printf("Bootloader addr: 0x%08lX\r\n", (unsigned long)BOOTLOADER_ADDR);
    printf("APP1 addr      : 0x%08lX\r\n", (unsigned long)BOOT_APP1_ADDR);
    printf("APP2 addr      : 0x%08lX\r\n", (unsigned long)BOOT_APP2_ADDR);
    printf("================================\r\n");

    HAL_Delay(300U);

    (void)Bootloader_ReadBootInfo(&boot_info);

    if ((boot_info.update_state == BOOT_UPDATE_STAGED) ||
        (boot_info.update_state == BOOT_UPDATE_INSTALLING))
    {
        printf("Pending update: active=%lu target=%lu state=%lu size=%lu\r\n",
               (unsigned long)boot_info.active_slot,
               (unsigned long)boot_info.target_slot,
               (unsigned long)boot_info.update_state,
               (unsigned long)boot_info.image_size);

        if (Bootloader_InstallPendingImage(&boot_info) == 0U)
        {
            printf("Install pending image failed\r\n");
            return;
        }
    }

    boot_slot = Bootloader_SelectBootSlot(&boot_info);
    if (boot_slot == BOOT_SLOT_APP1)
    {
        printf("Boot APP1\r\n");
        Bootloader_JumpToApp(BOOT_APP1_ADDR);
    }
    else if (boot_slot == BOOT_SLOT_APP2)
    {
        printf("Boot APP2\r\n");
        Bootloader_JumpToApp(BOOT_APP2_ADDR);
    }
    else
    {
        printf("No valid APP image\r\n");
        printf("Stay in bootloader\r\n");
    }
}