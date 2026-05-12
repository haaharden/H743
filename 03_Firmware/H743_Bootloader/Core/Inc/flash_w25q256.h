#ifndef __W25Q256_H
#define __W25Q256_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "quadspi.h"

#define W25Q256_OK                        ((uint8_t)0x00U)
#define W25Q256_ERROR                     ((uint8_t)0x01U)
#define W25Q256_BUSY                      ((uint8_t)0x02U)
#define W25Q256_NOT_SUPPORTED             ((uint8_t)0x03U)
#define W25Q256_INVALID_PARAM             ((uint8_t)0x04U)

#define W25Q256JV_JEDEC_ID                0xEF4019UL
#define W25Q256JV_CHIP_SIZE               (32UL * 1024UL * 1024UL)
#define W25Q256JV_PAGE_SIZE               256UL
#define W25Q256JV_ERASE_SIZE              0x1000UL
#define W25Q256JV_BULK_ERASE_MAX_TIME     250000UL
#define W25Q256JV_ERASE_MAX_TIME          5000UL

#define W25Q256_TOTAL_SIZE                W25Q256JV_CHIP_SIZE
#define W25Q256_ERASE_SIZE                W25Q256JV_ERASE_SIZE
#define W25Q256_PAGE_SIZE                 W25Q256JV_PAGE_SIZE

#define W25Q256_CMD_STD_READ_JEDEC_ID                 0x9FU
#define W25Q256_CMD_STD_WRITE_ENABLE                  0x06U
#define W25Q256_CMD_STD_READ_STATUS_REG1              0x05U
#define W25Q256_CMD_STATUS_READ_REG2                  0x35U
#define W25Q256_CMD_STD_WRITE_STATUS_REG1             0x01U
#define W25Q256_CMD_STATUS_WRITE_REG2                 0x31U
#define W25Q256_CMD_SPECIAL_ENABLE_RESET              0x66U
#define W25Q256_CMD_SPECIAL_RESET_DEVICE              0x99U
#define W25Q256_CMD_PROTECT_GLOBAL_BLOCK_UNLOCK       0x98U
#define W25Q256_CMD_ADDR4_ENTER_MODE                  0xB7U
#define W25Q256_CMD_QUAD_FAST_READ_OUTPUT_4BYTE       0x6CU
#define W25Q256_CMD_ADDR4_PAGE_PROGRAM                0x12U
#define W25Q256_CMD_ADDR4_SECTOR_ERASE_4KB            0x21U

#define W25Q256JV_SR_WIP                  0x01U
#define W25Q256JV_SR_WREN                 0x02U
#define W25Q256JV_SR_BP_MASK              0x1CU
#define W25Q256_SR2_QE_MASK               0x02U

uint8_t W25Q256_Init(void);
uint8_t W25Q256_ReadJedecId(uint32_t *jedec_id);
uint8_t W25Q256_Read(uint32_t address, uint8_t *buffer, uint32_t length);
uint8_t W25Q256_WritePage(uint32_t address, const uint8_t *buffer, uint32_t length);
uint8_t W25Q256_Write(uint32_t address, const uint8_t *buffer, uint32_t length);
uint8_t W25Q256_EraseSector(uint32_t address);
uint8_t W25Q256_WaitWhileBusy(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif