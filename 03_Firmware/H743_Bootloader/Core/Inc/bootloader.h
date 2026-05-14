#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/*
 * Internal flash layout
 *
 * Bootloader: 0x08000000, 128KB
 * APP1:       0x08020000, 896KB  (production image; OTA installs here)
 * APP2:       0x08100000, 896KB  (reserved; not used by this OTA flow)
 *
 * OTA uses external W25Q OTA_Flag_t dual copies + two firmware package slots
 * (see app ota_info.h / system_storge.h).
 */
#define BOOTLOADER_ADDR          0x08000000UL
#define BOOTLOADER_SIZE          0x00020000UL

#define BOOT_APP1_ADDR           0x08020000UL
#define BOOT_APP1_SIZE           0x000E0000UL

#define BOOT_APP2_ADDR           0x08100000UL
#define BOOT_APP2_SIZE           0x000E0000UL

#define BOOT_APP1_END_ADDR       (BOOT_APP1_ADDR + BOOT_APP1_SIZE)
#define BOOT_APP2_END_ADDR       (BOOT_APP2_ADDR + BOOT_APP2_SIZE)

#define BOOT_FLASH_END_ADDR      0x08200000UL

#define BOOT_FLASH_WORD_SIZE     32U
#define BOOT_INSTALL_CHUNK_SIZE  4096U

#define BOOT_APP1_BANK           FLASH_BANK_1
#define BOOT_APP1_FIRST_SECTOR   FLASH_SECTOR_1
#define BOOT_APP1_SECTOR_COUNT   7U

#define BOOT_APP2_BANK           FLASH_BANK_2
#define BOOT_APP2_FIRST_SECTOR   FLASH_SECTOR_0
#define BOOT_APP2_SECTOR_COUNT   7U

/*
 * STM32H743 ?? RAM ??
 */
#define BOOT_DTCM_RAM_START      0x20000000UL
#define BOOT_DTCM_RAM_END        0x20020000UL

#define BOOT_AXI_RAM_START       0x24000000UL
#define BOOT_AXI_RAM_END         0x24080000UL

#define BOOT_SRAM123_START       0x30000000UL
#define BOOT_SRAM123_END         0x30048000UL

#define BOOT_SRAM4_START         0x38000000UL
#define BOOT_SRAM4_END           0x38010000UL

typedef enum
{
	BOOT_SLOT_NONE = 0,
	BOOT_SLOT_APP1 = 1,
	BOOT_SLOT_APP2 = 2
} BootSlot_t;

void Bootloader_Run(void);

uint8_t Bootloader_IsAppValid(uint32_t app_addr, uint32_t app_size);
void Bootloader_JumpToApp(uint32_t app_addr);

#ifdef __cplusplus
}
#endif

#endif