#ifndef __FLASH_IF_H
#define __FLASH_IF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define APP2_FLASH_ADDR        0x08100000UL
#define APP2_FLASH_SIZE        0x000E0000UL
#define APP2_FLASH_END_ADDR    (APP2_FLASH_ADDR + APP2_FLASH_SIZE)

#define FLASH_WORD_SIZE        32U

typedef enum
{
    FLASH_IF_OK = 0,
    FLASH_IF_ADDR_ERROR,
    FLASH_IF_SIZE_ERROR,
    FLASH_IF_ERASE_ERROR,
    FLASH_IF_WRITE_ERROR,
    FLASH_IF_VERIFY_ERROR
} FlashIf_Result_t;

FlashIf_Result_t FlashIf_EraseApp2(void);
FlashIf_Result_t FlashIf_Write(uint32_t addr, const uint8_t *data, uint32_t len);
FlashIf_Result_t FlashIf_Verify(uint32_t addr, const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif