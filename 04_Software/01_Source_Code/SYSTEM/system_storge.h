#ifndef SYSTEM_STORGE_H
#define SYSTEM_STORGE_H

/*外部flash：w25q256，32mb*/
#define EXT_FLASH_SIZE          0x02000000UL   // 32MB

//LVGL资源区
#define RAWFS_BASE_ADDR         0x00000000UL
#define RAWFS_SIZE              0x01000000UL   // 16MB

//OTA固件区1 --- SLOT_A --- committed/staging rotates with SLOT_B below
#define OTA_NEW_FW_ADDR         0x01000000UL
#define OTA_NEW_FW_SIZE         0x00200000UL   // 2MB

//OTA固件区2 --- SLOT_B
#define OTA_OLD_FW_ADDR         0x01200000UL
#define OTA_OLD_FW_SIZE         0x00200000UL   // 2MB，可选

// Aliases after base addresses exist (Image_Info_t + payload per slot)
#define OTA_FW_SLOT_A_ADDR      OTA_NEW_FW_ADDR
#define OTA_FW_SLOT_A_SIZE      OTA_NEW_FW_SIZE
#define OTA_FW_SLOT_B_ADDR      OTA_OLD_FW_ADDR
#define OTA_FW_SLOT_B_SIZE      OTA_OLD_FW_SIZE

// OTA dual flag mirror (OTA_Flag_t) in distinct 4KB regions; bootloader & app pick higher seq valid copy.
#define OTA_FLAG_SIZE           0x00001000UL   // 4KB
#define OTA_FLAG1_ADDR          0x01400000UL
#define OTA_FLAG2_ADDR          (OTA_FLAG1_ADDR + OTA_FLAG_SIZE)


/*内部flash：共2MB*/
#define Bootloader_FLASH_ADDR   0x08000000UL
#define Bootloader_FLASH_SIZE   0x00020000UL   // 128KB

#define APP1_FLASH_ADDR         0x08020000UL
#define APP1_FLASH_SIZE         0x000E0000UL   // 896KB
#define APP1_FLASH_END_ADDR     (APP1_FLASH_ADDR + APP1_FLASH_SIZE)

#define APP2_FLASH_ADDR         0x08100000UL
#define APP2_FLASH_SIZE         0x000E0000UL   // 896KB
#define APP2_FLASH_END_ADDR     (APP2_FLASH_ADDR + APP2_FLASH_SIZE)

#endif