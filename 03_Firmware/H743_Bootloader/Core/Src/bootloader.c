#include "bootloader.h"
#include "crc32.h"
#include "flash_w25q256.h"
#include "ota_fw_slot.h"
#include "ota_info.h"
#include "quadspi.h"
#include "system_storge.h"
#include "usart.h"

#include <stdio.h>
#include <string.h>

static uint8_t s_install_buf[BOOT_INSTALL_CHUNK_SIZE];

typedef struct {
    OTA_Flag_t flag;
    uint32_t slot;
    uint32_t valid;
    uint32_t read_ok;
    uint32_t magic_ok;
} BlFlagInfo_t;

static uint8_t Bootloader_EraseSlot(uint32_t slot);
static uint8_t Bootloader_WriteImageToSlot(uint32_t slot, uint32_t ext_addr, uint32_t image_size);
static uint32_t Bootloader_GetSlotAddr(uint32_t slot);
static uint32_t Bootloader_GetSlotSize(uint32_t slot);
static uint32_t Bootloader_GetSlotBank(uint32_t slot);
static uint32_t Bootloader_GetSlotFirstSector(uint32_t slot);
static uint32_t Bootloader_GetSlotSectorCount(uint32_t slot);
static uint8_t Bootloader_IsValidSlot(uint32_t slot);
static uint8_t Bootloader_IsValidResetHandler(uint32_t pc,
                                              uint32_t app_addr,
                                              uint32_t app_size);
static uint8_t Bootloader_IsValidStack(uint32_t sp);

static int bl_is_allowed_erase_span(uint32_t address, uint32_t length);
static uint8_t Bootloader_EraseW25Range(uint32_t address, uint32_t length);
static void bl_read_flag_copy(uint32_t phys_addr, BlFlagInfo_t *info);
static void bl_check_flag_magic(BlFlagInfo_t *info);
static uint32_t bl_pick_winner_phys(const BlFlagInfo_t *f1, const BlFlagInfo_t *f2);
static uint32_t bl_inactive_flag_phys(uint32_t winner_phys);
static uint8_t Bootloader_OtaReadMerged(OTA_Flag_t *out);
static uint8_t Bootloader_OtaWriteDuplex(const OTA_Flag_t *flag);
static uint8_t Bootloader_ValidateExternalPackage(uint32_t pkg_addr, Image_Info_t *hdr_out);
static uint8_t Bootloader_InstallPackageToApp1(uint32_t pkg_addr, const Image_Info_t *hdr);
static void Bootloader_ProcessOtaFlags(void);

static uint8_t Bootloader_IsValidSlot(uint32_t slot)
{
    return ((slot == BOOT_SLOT_APP1) || (slot == BOOT_SLOT_APP2)) ? 1U : 0U;
}

static uint32_t Bootloader_GetSlotAddr(uint32_t slot)
{
    if (slot == BOOT_SLOT_APP1) {
        return BOOT_APP1_ADDR;
    }

    if (slot == BOOT_SLOT_APP2) {
        return BOOT_APP2_ADDR;
    }

    return 0U;
}

static uint32_t Bootloader_GetSlotSize(uint32_t slot)
{
    if (slot == BOOT_SLOT_APP1) {
        return BOOT_APP1_SIZE;
    }

    if (slot == BOOT_SLOT_APP2) {
        return BOOT_APP2_SIZE;
    }

    return 0U;
}

static uint32_t Bootloader_GetSlotBank(uint32_t slot)
{
    if (slot == BOOT_SLOT_APP1) {
        return BOOT_APP1_BANK;
    }

    if (slot == BOOT_SLOT_APP2) {
        return BOOT_APP2_BANK;
    }

    return 0U;
}

static uint32_t Bootloader_GetSlotFirstSector(uint32_t slot)
{
    if (slot == BOOT_SLOT_APP1) {
        return BOOT_APP1_FIRST_SECTOR;
    }

    if (slot == BOOT_SLOT_APP2) {
        return BOOT_APP2_FIRST_SECTOR;
    }

    return 0U;
}

static uint32_t Bootloader_GetSlotSectorCount(uint32_t slot)
{
    if (slot == BOOT_SLOT_APP1) {
        return BOOT_APP1_SECTOR_COUNT;
    }

    if (slot == BOOT_SLOT_APP2) {
        return BOOT_APP2_SECTOR_COUNT;
    }

    return 0U;
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

    if ((erase.Banks == 0U) || (erase.NbSectors == 0U)) {
        return 0U;
    }

    HAL_FLASH_Unlock();
    sector_error = 0U;
    status = HAL_FLASHEx_Erase(&erase, &sector_error);
    HAL_FLASH_Lock();

    if (status != HAL_OK) {
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
    if ((slot_addr == 0U) || (image_size == 0U)) {
        return 0U;
    }

    if (Bootloader_EraseSlot(slot) == 0U) {
        return 0U;
    }

    HAL_FLASH_Unlock();

    offset = 0U;
    while (offset < image_size) {
        chunk_size = image_size - offset;
        if (chunk_size > BOOT_INSTALL_CHUNK_SIZE) {
            chunk_size = BOOT_INSTALL_CHUNK_SIZE;
        }

        if (W25Q256_Read(ext_addr + offset, s_install_buf, chunk_size) != W25Q256_OK) {
            HAL_FLASH_Lock();
            printf("Read staged image failed, offset=%lu\r\n",
                   (unsigned long)offset);
            return 0U;
        }

        program_offset = 0U;
        while (program_offset < chunk_size) {
            write_len = chunk_size - program_offset;
            if (write_len > BOOT_FLASH_WORD_SIZE) {
                write_len = BOOT_FLASH_WORD_SIZE;
            }

            memset(flash_word, 0xFF, sizeof(flash_word));
            memcpy((uint8_t *)flash_word, &s_install_buf[program_offset], write_len);

            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                                       slot_addr + offset + program_offset,
                                       (uint32_t)flash_word);
            if (status != HAL_OK) {
                HAL_FLASH_Lock();
                printf("Program slot %lu failed at 0x%08lX, err=0x%08lX\r\n",
                       (unsigned long)slot,
                       (unsigned long)(slot_addr + offset + program_offset),
                       (unsigned long)HAL_FLASH_GetError());
                return 0U;
            }

            program_offset += BOOT_FLASH_WORD_SIZE;
        }

        if (memcmp((const void *)(slot_addr + offset), s_install_buf, chunk_size) != 0) {
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

static int bl_is_allowed_erase_span(uint32_t address, uint32_t length)
{
    uint32_t end;

    if ((length == 0U) || ((address + length) < address)) {
        return 0;
    }

    end = address + length;

    if ((address >= OTA_FLAG1_ADDR) && (end <= (OTA_FLAG1_ADDR + OTA_FLAG_SIZE))) {
        return 1;
    }

    if ((address >= OTA_FLAG2_ADDR) && (end <= (OTA_FLAG2_ADDR + OTA_FLAG_SIZE))) {
        return 1;
    }

    if ((address >= OTA_FW_SLOT_A_ADDR) &&
        (address < (OTA_FW_SLOT_A_ADDR + OTA_FW_SLOT_A_SIZE)) &&
        (end <= (OTA_FW_SLOT_A_ADDR + OTA_FW_SLOT_A_SIZE))) {
        return 1;
    }

    if ((address >= OTA_FW_SLOT_B_ADDR) &&
        (address < (OTA_FW_SLOT_B_ADDR + OTA_FW_SLOT_B_SIZE)) &&
        (end <= (OTA_FW_SLOT_B_ADDR + OTA_FW_SLOT_B_SIZE))) {
        return 1;
    }

    return 0;
}

static uint8_t Bootloader_EraseW25Range(uint32_t address, uint32_t length)
{
    uint32_t erase_addr;
    uint32_t erase_end;

    if (bl_is_allowed_erase_span(address, length) == 0) {
        return 0U;
    }

    erase_addr = address - (address % W25Q256_ERASE_SIZE);
    erase_end = address + length;
    erase_end = (erase_end + W25Q256_ERASE_SIZE - 1U) & ~(W25Q256_ERASE_SIZE - 1U);

    while (erase_addr < erase_end) {
        if (W25Q256_EraseSector(erase_addr) != W25Q256_OK) {
            printf("Ext flash erase failed at 0x%08lX\r\n", (unsigned long)erase_addr);
            return 0U;
        }
        erase_addr += W25Q256_ERASE_SIZE;
    }

    return 1U;
}

static void bl_read_flag_copy(uint32_t phys_addr, BlFlagInfo_t *info)
{
    if (info == NULL) {
        return;
    }

    info->slot = phys_addr;
    info->read_ok = 0U;
    info->magic_ok = 0U;
    info->valid = 0U;

    if (W25Q256_Read(phys_addr, (uint8_t *)&info->flag, sizeof(OTA_Flag_t)) != W25Q256_OK) {
        return;
    }
    info->read_ok = 1U;

    bl_check_flag_magic(info);
    if (info->magic_ok == 0U) {
        return;
    }

    info->valid = 1U;
}

static void bl_check_flag_magic(BlFlagInfo_t *info)
{
    if (info == NULL) {
        return;
    }

    info->magic_ok = (info->flag.magic == OTA_FLAG_MAGIC) ? 1U : 0U;
}

static uint32_t bl_pick_winner_phys(const BlFlagInfo_t *f1, const BlFlagInfo_t *f2)
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

static uint32_t bl_inactive_flag_phys(uint32_t winner_phys)
{
    if (winner_phys == OTA_FLAG1_ADDR) {
        return OTA_FLAG2_ADDR;
    }

    return OTA_FLAG1_ADDR;
}

static uint8_t Bootloader_OtaReadMerged(OTA_Flag_t *out)
{
    BlFlagInfo_t f1 = {0};
    BlFlagInfo_t f2 = {0};

    if (out == NULL) {
        return 0U;
    }

    bl_read_flag_copy(OTA_FLAG1_ADDR, &f1);
    bl_read_flag_copy(OTA_FLAG2_ADDR, &f2);

    if ((f1.valid != 1U) && (f2.valid != 1U)) {
        return 0U;
    }

    if ((f1.valid == 1U) && (f2.valid != 1U)) {
        memcpy(out, &f1.flag, sizeof(OTA_Flag_t));
        return 1U;
    }

    if ((f1.valid != 1U) && (f2.valid == 1U)) {
        memcpy(out, &f2.flag, sizeof(OTA_Flag_t));
        return 1U;
    }

    memcpy(out,
           (f1.flag.seq >= f2.flag.seq) ? &f1.flag : &f2.flag,
           sizeof(OTA_Flag_t));
    return 1U;
}

static uint8_t Bootloader_OtaWriteDuplex(const OTA_Flag_t *flag)
{
    BlFlagInfo_t f1 = {0};
    BlFlagInfo_t f2 = {0};
    uint32_t winner;
    uint32_t inactive;
    OTA_Flag_t wb;

    if (flag == NULL) {
        return 0U;
    }

    bl_read_flag_copy(OTA_FLAG1_ADDR, &f1);
    bl_read_flag_copy(OTA_FLAG2_ADDR, &f2);

    winner = bl_pick_winner_phys(&f1, &f2);
    inactive = bl_inactive_flag_phys((winner == 0U) ? OTA_FLAG2_ADDR : winner);

    memcpy(&wb, flag, sizeof(wb));
    wb.flag_crc32 = 0U;

    if (Bootloader_EraseW25Range(inactive, OTA_FLAG_SIZE) == 0U) {
        return 0U;
    }

    if (W25Q256_Write(inactive, (uint8_t *)&wb, sizeof(wb)) != W25Q256_OK) {
        return 0U;
    }

    {
        OTA_Flag_t verify;
        if (W25Q256_Read(inactive, (uint8_t *)&verify, sizeof(verify)) != W25Q256_OK) {
            return 0U;
        }
        if (memcmp(&verify, &wb, sizeof(verify)) != 0) {
            return 0U;
        }
    }

    return 1U;
}

static uint8_t Bootloader_ValidateExternalPackage(uint32_t pkg_addr, Image_Info_t *hdr_out)
{
    Image_Info_t hdr;
    uint32_t hdr_sz;
    uint32_t offset;
    uint32_t chunk;
    uint32_t crc_ctx;
    uint32_t crc_final;
    uint32_t cap;
    uint32_t prefix[2];

    hdr_sz = (uint32_t)sizeof(Image_Info_t);

    if (W25Q256_Read(pkg_addr, (uint8_t *)&hdr, sizeof(hdr)) != W25Q256_OK) {
        return 0U;
    }

    if (hdr.magic != Image_INFO_MAGIC) {
        return 0U;
    }

    if ((hdr.image_size == 0U) || (hdr.image_size > BOOT_APP1_SIZE)) {
        return 0U;
    }

    if (pkg_addr == OTA_FW_SLOT_A_ADDR) {
        cap = OTA_FW_SLOT_A_SIZE;
    } else if (pkg_addr == OTA_FW_SLOT_B_ADDR) {
        cap = OTA_FW_SLOT_B_SIZE;
    } else {
        return 0U;
    }

    if ((hdr_sz + hdr.image_size) > cap) {
        return 0U;
    }

    if ((pkg_addr + hdr_sz + hdr.image_size) > EXT_FLASH_SIZE ||
        (pkg_addr + hdr_sz + hdr.image_size) < pkg_addr) {
        return 0U;
    }

    if (W25Q256_Read(pkg_addr + hdr_sz, (uint8_t *)prefix, sizeof(prefix)) != W25Q256_OK) {
        return 0U;
    }

    if (Bootloader_IsValidStack(prefix[0]) == 0U) {
        return 0U;
    }

    if (Bootloader_IsValidResetHandler(prefix[1], BOOT_APP1_ADDR, BOOT_APP1_SIZE) == 0U) {
        return 0U;
    }

    CRC32_Init(&crc_ctx);
    offset = 0U;
    while (offset < hdr.image_size) {
        chunk = hdr.image_size - offset;
        if (chunk > BOOT_INSTALL_CHUNK_SIZE) {
            chunk = BOOT_INSTALL_CHUNK_SIZE;
        }

        if (W25Q256_Read(pkg_addr + hdr_sz + offset, s_install_buf, chunk) != W25Q256_OK) {
            return 0U;
        }

        CRC32_Update(&crc_ctx, s_install_buf, chunk);
        offset += chunk;
    }

    crc_final = CRC32_Final(&crc_ctx);
    if (crc_final != hdr.image_crc32) {
        printf("Package CRC mismatch\r\n");
        return 0U;
    }

    if (hdr_out != NULL) {
        memcpy(hdr_out, &hdr, sizeof(hdr));
    }

    return 1U;
}

static uint8_t Bootloader_InstallPackageToApp1(uint32_t pkg_addr, const Image_Info_t *hdr)
{
    uint32_t hdr_sz;
    uint32_t payload_addr;

    if (hdr == NULL) {
        return 0U;
    }

    hdr_sz = (uint32_t)sizeof(Image_Info_t);
    payload_addr = pkg_addr + hdr_sz;

    return Bootloader_WriteImageToSlot(BOOT_SLOT_APP1, payload_addr, hdr->image_size);
}

static void Bootloader_ProcessOtaFlags(void)
{
    OTA_Flag_t fl;
    Image_Info_t hdr;
    uint32_t pkg;

    if (Bootloader_OtaReadMerged(&fl) == 0U) {
        return;
    }

    switch (fl.ota_state) {
    case OTA_DOWNLOADED: {
        if ((fl.staging_slot != OTA_FW_SLOT_A) && (fl.staging_slot != OTA_FW_SLOT_B)) {
            printf("OTA_DOWNLOADED bad staging_slot=%lu\r\n",
                   (unsigned long)fl.staging_slot);
            fl.ota_state = OTA_FAILED;
            fl.last_result = 1U;
            fl.seq++;
            (void)Bootloader_OtaWriteDuplex(&fl);
            return;
        }

        pkg = OTA_Fw_Slot_BaseAddr(fl.staging_slot);
        if (pkg == 0U) {
            return;
        }

        if (Bootloader_ValidateExternalPackage(pkg, &hdr) == 0U) {
            printf("Staged package invalid\r\n");
            fl.ota_state = OTA_FAILED;
            fl.last_result = 2U;
            fl.seq++;
            (void)Bootloader_OtaWriteDuplex(&fl);
            return;
        }

        if (Bootloader_InstallPackageToApp1(pkg, &hdr) == 0U) {
            printf("Install to APP1 failed\r\n");
            fl.ota_state = OTA_FAILED;
            fl.last_result = 3U;
            fl.seq++;
            (void)Bootloader_OtaWriteDuplex(&fl);
            return;
        }

        fl.ota_state = OTA_INSTALLED;
        fl.boot_count = 0U;
        fl.last_result = 0U;
        fl.seq++;
        if (Bootloader_OtaWriteDuplex(&fl) == 0U) {
            printf("Failed to persist OTA_INSTALLED flag\r\n");
        } else {
            printf("OTA installed from staging, awaiting APP confirm\r\n");
        }
        return;
    }

    case OTA_INSTALLED: {
        fl.boot_count++;
        if (fl.boot_count >= OTA_TRIAL_BOOT_MAX) {
            uint32_t rb_slot;
            uint32_t rb_pkg;

            printf("OTA trial limit, rolling back to committed slot=%lu\r\n",
                   (unsigned long)fl.committed_slot);

            if ((fl.committed_slot != OTA_FW_SLOT_A) && (fl.committed_slot != OTA_FW_SLOT_B)) {
                printf("No valid committed_slot for rollback\r\n");
                fl.ota_state = OTA_FAILED;
                fl.last_result = 4U;
                fl.seq++;
                (void)Bootloader_OtaWriteDuplex(&fl);
                return;
            }

            rb_slot = fl.committed_slot;
            rb_pkg = OTA_Fw_Slot_BaseAddr(rb_slot);
            if ((rb_pkg == 0U) ||
                (Bootloader_ValidateExternalPackage(rb_pkg, &hdr) == 0U)) {
                printf("Rollback package invalid\r\n");
                fl.ota_state = OTA_FAILED;
                fl.last_result = 5U;
                fl.seq++;
                (void)Bootloader_OtaWriteDuplex(&fl);
                return;
            }

            if (Bootloader_InstallPackageToApp1(rb_pkg, &hdr) == 0U) {
                printf("Rollback install failed\r\n");
                fl.ota_state = OTA_FAILED;
                fl.last_result = 6U;
                fl.seq++;
                (void)Bootloader_OtaWriteDuplex(&fl);
                return;
            }

            fl.ota_state = OTA_IDLE;
            fl.staging_slot = OTA_FW_SLOT_NONE;
            fl.boot_count = 0U;
            fl.last_result = 0U;
            fl.seq++;
            (void)Bootloader_OtaWriteDuplex(&fl);
            printf("Rollback complete\r\n");
            return;
        }

        fl.seq++;
        if (Bootloader_OtaWriteDuplex(&fl) == 0U) {
            printf("Failed to update trial boot_count\r\n");
        }
        return;
    }

    default:
        return;
    }
}

static uint8_t Bootloader_IsValidStack(uint32_t sp)
{
    if ((sp & 0x7U) != 0U) {
        return 0U;
    }

    if ((sp > BOOT_DTCM_RAM_START) && (sp <= BOOT_DTCM_RAM_END)) {
        return 1U;
    }

    if ((sp > BOOT_AXI_RAM_START) && (sp <= BOOT_AXI_RAM_END)) {
        return 1U;
    }

    if ((sp > BOOT_SRAM123_START) && (sp <= BOOT_SRAM123_END)) {
        return 1U;
    }

    if ((sp > BOOT_SRAM4_START) && (sp <= BOOT_SRAM4_END)) {
        return 1U;
    }

    return 0U;
}

static uint8_t Bootloader_IsValidResetHandler(uint32_t pc,
                                              uint32_t app_addr,
                                              uint32_t app_size)
{
    uint32_t real_pc;

    if ((pc & 0x1U) == 0U) {
        return 0U;
    }

    real_pc = pc & 0xFFFFFFFEUL;

    if ((real_pc >= app_addr) && (real_pc < (app_addr + app_size))) {
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

    if (Bootloader_IsValidStack(app_sp) == 0U) {
        return 0U;
    }

    if (Bootloader_IsValidResetHandler(app_pc, app_addr, app_size) == 0U) {
        return 0U;
    }

    return 1U;
}

static void Bootloader_DeInitBeforeJump(void)
{
    HAL_UART_DeInit(&huart1);
    HAL_QSPI_DeInit(&hqspi);

    HAL_RCC_DeInit();

    HAL_DeInit();

    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    {
        uint32_t group_count;
        uint32_t i;

        group_count = ((SCnSCB->ICTR & 0xFU) + 1U);

        for (i = 0U; i < group_count; i++) {
            NVIC->ICER[i] = 0xFFFFFFFFU;
            NVIC->ICPR[i] = 0xFFFFFFFFU;
        }
    }

    __set_BASEPRI(0U);
    __set_FAULTMASK(0U);

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

    HAL_Delay(20U);

    Bootloader_DeInitBeforeJump();

    SCB->VTOR = app_addr;

    __set_CONTROL(0U);

    __set_MSP(app_sp);

    __DSB();
    __ISB();

    __enable_irq();

    app_entry();

    while (1) {
    }
}

void Bootloader_Run(void)
{
    printf("\r\n");
    printf("================================\r\n");
    printf("STM32H743 Bootloader start\r\n");
    printf("Bootloader addr: 0x%08lX\r\n", (unsigned long)BOOTLOADER_ADDR);
    printf("APP1 addr      : 0x%08lX\r\n", (unsigned long)BOOT_APP1_ADDR);
    printf("================================\r\n");

    HAL_Delay(300U);

    MX_QUADSPI_Init();
    if (W25Q256_Init() != W25Q256_OK) {
        printf("W25Q256 init failed, skip OTA flag handling\r\n");
    } else {
        Bootloader_ProcessOtaFlags();
    }

    if (Bootloader_IsAppValid(BOOT_APP1_ADDR, BOOT_APP1_SIZE) != 0U) {
        printf("Boot APP1\r\n");
        Bootloader_JumpToApp(BOOT_APP1_ADDR);
    }

    printf("No valid APP1 image\r\n");
    printf("Stay in bootloader\r\n");
}
