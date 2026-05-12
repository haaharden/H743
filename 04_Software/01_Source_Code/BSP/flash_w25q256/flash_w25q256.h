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

// 单颗 W25Q256JV 参数。
#define W25Q256JV_JEDEC_ID                 0xEF4019UL
#define W25Q256JV_CHIP_SIZE                (32UL * 1024UL * 1024UL)
#define W25Q256JV_PAGE_SIZE                256UL
#define W25Q256JV_ERASE_SIZE               0x1000UL   
#define W25Q256JV_BULK_ERASE_MAX_TIME      250000UL
#define W25Q256JV_ERASE_MAX_TIME           5000UL

#define W25Q256_TOTAL_SIZE                 W25Q256JV_CHIP_SIZE
#define W25Q256_ERASE_SIZE                 W25Q256JV_ERASE_SIZE
#define W25Q256_PAGE_SIZE                  W25Q256JV_PAGE_SIZE

typedef struct {
  uint32_t JedecId;
  uint32_t FlashSize;
  uint32_t EraseSectorSize;
  uint32_t EraseSectorsNumber;
  uint32_t ProgPageSize;
  uint32_t ProgPagesNumber;
} W25Q256_Info;

// 一、标准 SPI 命令（Standard SPI）。
#define W25Q256_CMD_STD_READ_DATA                          0x03U  // 标准读数据命令，3 字节地址，支持连续读取。
#define W25Q256_CMD_STD_FAST_READ_DATA                     0x0BU  // 标准快速读命令，3 字节地址，带 dummy 周期提升读取速度。
#define W25Q256_CMD_STD_PAGE_PROGRAM                       0x02U  // 标准页编程命令，3 字节地址，单次最多编程 256 字节。
#define W25Q256_CMD_STD_SECTOR_ERASE_4KB                   0x20U  // 标准 4KB 扇区擦除命令，3 字节地址。
#define W25Q256_CMD_STD_BLOCK_ERASE_32KB                   0x52U  // 标准 32KB 块擦除命令，3 字节地址。
#define W25Q256_CMD_STD_BLOCK_ERASE_64KB                   0xD8U  // 标准 64KB 块擦除命令，3 字节地址。
#define W25Q256_CMD_STD_CHIP_ERASE_C7                      0xC7U  // 标准整片擦除命令写法之一，擦除整个芯片。
#define W25Q256_CMD_STD_CHIP_ERASE_60                      0x60U  // 标准整片擦除命令写法之二，作用与 0xC7 相同。
#define W25Q256_CMD_STD_WRITE_ENABLE                       0x06U  // 标准写使能命令，后续编程或擦除前必须先发送。
#define W25Q256_CMD_STD_WRITE_DISABLE                      0x04U  // 标准写禁止命令，用于清除写使能状态。
#define W25Q256_CMD_STD_READ_STATUS_REG1                   0x05U  // 标准读状态寄存器 1 命令，可读取 BUSY、WEL 等状态位。
#define W25Q256_CMD_STD_WRITE_STATUS_REG1                  0x01U  // 标准写状态寄存器 1 命令，可配置 BP、TB 等保护位。
#define W25Q256_CMD_STD_POWER_DOWN                         0xB9U  // 标准掉电命令，让器件进入低功耗模式。
#define W25Q256_CMD_STD_RELEASE_POWER_DOWN_OR_READ_ID      0xABU  // 释放掉电状态或读取设备 ID 命令。
#define W25Q256_CMD_STD_READ_MANUFACTURER_DEVICE_ID        0x90U  // 标准读取厂商 ID 和设备 ID 命令。
#define W25Q256_CMD_STD_READ_JEDEC_ID                      0x9FU  // 标准读取 JEDEC ID 命令，典型返回 EF 40 19。

// 二、四字节地址命令（4-Byte Address Mode）。
#define W25Q256_CMD_ADDR4_READ_DATA                        0x13U  // 4 字节地址普通读命令，适合访问 16MB 以上空间。
#define W25Q256_CMD_ADDR4_FAST_READ_DATA                   0x0CU  // 4 字节地址快速读命令，带 dummy 周期。
#define W25Q256_CMD_ADDR4_PAGE_PROGRAM                     0x12U  // 4 字节地址页编程命令，单次最多 256 字节。
#define W25Q256_CMD_ADDR4_SECTOR_ERASE_4KB                 0x21U  // 4 字节地址 4KB 扇区擦除命令。
#define W25Q256_CMD_ADDR4_BLOCK_ERASE_64KB                 0xDCU  // 4 字节地址 64KB 块擦除命令。
#define W25Q256_CMD_ADDR4_ENTER_MODE                       0xB7U  // 进入 4 字节地址模式命令。
#define W25Q256_CMD_ADDR4_EXIT_MODE                        0xE9U  // 退出 4 字节地址模式命令，回到 3 字节地址模式。

// 三、双 SPI 命令（Dual SPI）。
#define W25Q256_CMD_DUAL_FAST_READ_OUTPUT                  0x3BU  // 双输出快速读命令，3 字节地址，数据阶段走 IO0/IO1。
#define W25Q256_CMD_DUAL_FAST_READ_IO                      0xBBU  // 双 I/O 快速读命令，地址输入和数据输出都使用双线。
#define W25Q256_CMD_DUAL_FAST_READ_OUTPUT_4BYTE            0x3CU  // 4 字节地址双输出快速读命令。
#define W25Q256_CMD_DUAL_FAST_READ_IO_4BYTE                0xBCU  // 4 字节地址双 I/O 快速读命令。
#define W25Q256_CMD_DUAL_READ_MANUFACTURER_DEVICE_ID       0x92U  // 双 I/O 读取厂商 ID 和设备 ID 命令。

// 四、四 SPI 命令（Quad SPI）。
#define W25Q256_CMD_QUAD_FAST_READ_OUTPUT                  0x6BU  // 四输出快速读命令，3 字节地址，数据阶段走四线。
#define W25Q256_CMD_QUAD_FAST_READ_IO                      0xEBU  // 四 I/O 快速读命令，地址和数据都走四线。
#define W25Q256_CMD_QUAD_PAGE_PROGRAM                      0x32U  // 四线输入页编程命令，3 字节地址。
#define W25Q256_CMD_QUAD_FAST_READ_OUTPUT_4BYTE            0x6CU  // 4 字节地址四输出快速读命令。
#define W25Q256_CMD_QUAD_FAST_READ_IO_4BYTE                0xECU  // 4 字节地址四 I/O 快速读命令。
#define W25Q256_CMD_QUAD_PAGE_PROGRAM_4BYTE                0x34U  // 4 字节地址四线输入页编程命令。
#define W25Q256_CMD_QUAD_READ_MANUFACTURER_DEVICE_ID       0x94U  // 四 I/O 读取厂商 ID 和设备 ID 命令。
#define W25Q256_CMD_QUAD_SET_BURST_WRAP                    0x77U  // 设置 Quad 连续读的 wrap 突发长度。

// 五、安全与保护命令。
#define W25Q256_CMD_PROTECT_READ_SECURITY_REGISTER         0x48U  // 读取安全寄存器命令，可访问 3 个 256B 安全区。
#define W25Q256_CMD_PROTECT_PROGRAM_SECURITY_REGISTER      0x42U  // 编程安全寄存器命令，写前通常需要先擦除。
#define W25Q256_CMD_PROTECT_ERASE_SECURITY_REGISTER        0x44U  // 擦除安全寄存器命令。
#define W25Q256_CMD_PROTECT_INDIVIDUAL_BLOCK_LOCK          0x36U  // 单独块或扇区锁定命令，WPS=1 时生效。
#define W25Q256_CMD_PROTECT_INDIVIDUAL_BLOCK_UNLOCK        0x39U  // 单独块或扇区解锁命令。
#define W25Q256_CMD_PROTECT_GLOBAL_BLOCK_LOCK              0x7EU  // 全局锁定所有块或扇区命令。
#define W25Q256_CMD_PROTECT_GLOBAL_BLOCK_UNLOCK            0x98U  // 全局解锁所有块或扇区命令。

// 六、状态与配置命令。
#define W25Q256_CMD_STATUS_READ_REG2                       0x35U  // 读取状态寄存器 2 命令，可读取 QE、SUS 等位。
#define W25Q256_CMD_STATUS_WRITE_REG2                      0x31U  // 写状态寄存器 2 命令，可配置 QE、LB 等位。
#define W25Q256_CMD_STATUS_READ_REG3                       0x15U  // 读取状态寄存器 3 命令，可读取 ADS、ADP 等位。
#define W25Q256_CMD_STATUS_WRITE_REG3                      0x11U  // 写状态寄存器 3 命令，可配置 WPS、DRV 等位。
#define W25Q256_CMD_STATUS_READ_EXTENDED_ADDRESS_REG       0xC8U  // 读取扩展地址寄存器命令，常用于 3 字节地址模式下读取 A24。
#define W25Q256_CMD_STATUS_WRITE_EXTENDED_ADDRESS_REG      0xC5U  // 写扩展地址寄存器命令，配置 3 字节地址模式下的高位地址。
#define W25Q256_CMD_STATUS_READ_SFDP                       0x5AU  // 读取 SFDP 参数表命令，用于发现器件能力。

// 七、复位与特殊命令。
#define W25Q256_CMD_SPECIAL_ENABLE_RESET                   0x66U  // 软件复位使能命令，需要和复位命令配对使用。
#define W25Q256_CMD_SPECIAL_RESET_DEVICE                   0x99U  // 软件复位命令，让器件恢复到默认状态。
#define W25Q256_CMD_SPECIAL_PROGRAM_ERASE_SUSPEND          0x75U  // 挂起当前擦除或编程操作命令。
#define W25Q256_CMD_SPECIAL_PROGRAM_ERASE_RESUME           0x7AU  // 恢复此前被挂起的擦除或编程操作命令。
#define W25Q256_CMD_SPECIAL_READ_UNIQUE_ID                 0x4BU  // 读取 64 位唯一 ID 命令。

// SR1 位定义。
#define W25Q256JV_SR_WIP                   0x01U
#define W25Q256JV_SR_WREN                  0x02U
#define W25Q256JV_SR_BP_MASK               0x1CU
#define W25Q256_SR2_QE_MASK                0x02U

uint8_t W25Q256_Init(void);
uint8_t W25Q256_ReadJedecId(uint32_t *jedec_id);
uint8_t W25Q256_Read(uint32_t address, uint8_t *buffer, uint32_t length);
uint8_t W25Q256_WritePage(uint32_t address, const uint8_t *buffer, uint32_t length);
uint8_t W25Q256_Write(uint32_t address, const uint8_t *buffer, uint32_t length);
uint8_t W25Q256_EraseSector(uint32_t address);
uint8_t W25Q256_WaitWhileBusy(uint32_t timeout_ms);
uint8_t W25Q256_GetStatus(void);
void W25Q256_GetInfo(W25Q256_Info *info);
uint8_t W25Q256_BasicTest(void);

#ifdef __cplusplus
}
#endif


#endif