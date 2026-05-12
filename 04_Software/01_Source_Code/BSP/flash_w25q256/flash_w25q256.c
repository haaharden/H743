#include "flash_w25q256.h"  // 对外接口、命令码、容量定义都在头文件里声明。
#include <string.h>

static uint8_t g_w25q256_ready = 0U;  // 软件侧初始化完成标志，避免重复做复位和模式配置。

static uint8_t W25Q256_ResetMemory(void);                             // 发送复位序列，让器件回到已知初始状态。
static uint8_t W25Q256_WriteEnable(void);                             // 发送写使能命令，并确认 WREN 位已经置上。
static uint8_t W25Q256_GlobalBlockUnlock(void);                       // 全局解除块保护，避免后续写擦被保护位拦住。
static uint8_t W25Q256_WriteStatusRegister1(uint8_t sr1);             // 单独写状态寄存器 1，主要用于清保护位。
static uint8_t W25Q256_WriteStatusRegister2(uint8_t sr2);             // 单独写状态寄存器 2，主要用于设置 QE 位。
static uint8_t W25Q256_ReadStatusRegister1(uint8_t *status);          // 读取状态寄存器 1，用于轮询 WIP/WREN 等位。
static uint8_t W25Q256_ReadStatusRegister2(uint8_t *status);

uint8_t W25Q256_Init(void)
{
  uint8_t sr1 = 0U;  // 暂存状态寄存器 1 的值，用来检查保护位是否已经清掉。
  uint8_t sr2 = 0U;  // 暂存状态寄存器 2 的值，用来确认 QE 位已经真正写成功。

  if (g_w25q256_ready != 0U) {  // 如果软件已经完成初始化，就直接返回成功，避免重复配置。
    return W25Q256_OK;
  }

  if (W25Q256_ResetMemory() != W25Q256_OK) {  // 先把芯片复位到确定状态，避免上电残留模式影响后续命令。
    return W25Q256_NOT_SUPPORTED;
  }

  if (W25Q256_GlobalBlockUnlock() != W25Q256_OK) {  // 解除全局保护，否则擦写可能因为 BP 位而失败。
    return W25Q256_ERROR;
  }

  if (W25Q256_WriteStatusRegister1(0x00U) != W25Q256_OK) {  // 先单独清掉 SR1 里的保护位，避免后续写擦被 BP 位拦住。
    return W25Q256_ERROR;
  }

  if (W25Q256_WriteStatusRegister2(W25Q256_SR2_QE_MASK) != W25Q256_OK) {  // 再单独写 SR2，把 QE 位置 1，开启 Quad 模式。
    return W25Q256_ERROR;
  }

  if (W25Q256_ReadStatusRegister1(&sr1) != W25Q256_OK) {  // 读回 SR1，确认状态寄存器访问正常。
    return W25Q256_ERROR;
  }

  if ((sr1 & W25Q256JV_SR_BP_MASK) != 0U) {  // 只要 BP 保护位还有值，就说明解除保护没有生效。
    return W25Q256_ERROR;
  }

  if (W25Q256_ReadStatusRegister2(&sr2) != W25Q256_OK) {  // 再读回 SR2，确认 QE 位已经置上，否则四线读写会失效。
    return W25Q256_ERROR;
  }

  if ((sr2 & W25Q256_SR2_QE_MASK) == 0U) {  // QE 位为 0 说明器件还没进入 Quad 使能状态。
    return W25Q256_ERROR;
  }

  g_w25q256_ready = 1U;  // 走到这里说明初始化流程全部完成，后续可直接读写。
  return W25Q256_OK;     // 对外返回初始化成功。
}

uint8_t W25Q256_ReadJedecId(uint32_t *jedec_id)
{
  QSPI_CommandTypeDef command = {0};  // 每次都从零构造一条完整命令，避免沿用旧字段。
  uint8_t data[3] = {0};              // JEDEC ID 固定返回 3 字节：厂家、存储类型、容量编码。

  if (jedec_id == 0) {  // 输出指针为空时无法回传结果，直接报参数错误。
    return W25Q256_INVALID_PARAM;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;     // 指令阶段走 1 线，这是 JEDEC ID 的标准发法。
  command.Instruction = W25Q256_CMD_STD_READ_JEDEC_ID;   // 指令码 0x9F，用来读取 JEDEC ID。
  command.AddressMode = QSPI_ADDRESS_NONE;               // 读id不需要地址，这条命令表示地址阶段用几根线
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; // 该命令不需要交替字节。
  command.DataMode = QSPI_DATA_1_LINE;                   // 返回数据用 1 线接收。
  command.DummyCycles = 0;                               // JEDEC ID 命令不需要 dummy 周期。
  command.NbData = sizeof(data);                         // 总共读取 3 字节数据。
  command.DdrMode = QSPI_DDR_MODE_DISABLE;               // 当前驱动不用 DDR 模式。
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  // HAL 要求给出 DDR 保持配置，即使当前未启用 DDR。
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           // 每次命令都重新发送 instruction。

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {  // 先把命令帧发给 QSPI 外设。
    return W25Q256_ERROR;
  }

  if (HAL_QSPI_Receive(&hqspi, data, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {  // 再从数据线收回 JEDEC ID。
    return W25Q256_ERROR;
  }

  *jedec_id = (((uint32_t)data[0]) << 16) | (((uint32_t)data[1]) << 8) | data[2];  // 把 3 字节拼成一个 24 位 ID 结果。
  return W25Q256_OK;  // 读取成功。
}

uint8_t W25Q256_Read(uint32_t address, uint8_t *buffer, uint32_t length)
{
  QSPI_CommandTypeDef command = {0};  // 读命令的完整配置结构体。

  if (length == 0U) {  // 读长度为 0 时按成功处理，避免上层还要特判。
    return W25Q256_OK;
  }

  if (buffer == 0 || (address + length) > W25Q256_TOTAL_SIZE) {  // 指针为空或访问越界都属于非法参数。
    return W25Q256_INVALID_PARAM;
  }

  if (W25Q256_Init() != W25Q256_OK) {  // 确保芯片已经完成复位、解锁和地址模式配置。
    return W25Q256_ERROR;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;     // 读命令的 instruction 阶段按 1 线发送。
  command.Instruction = W25Q256_CMD_QUAD_FAST_READ_OUTPUT_4BYTE;   // 四输出快速读指令 0x6C。
  command.AddressMode = QSPI_ADDRESS_1_LINE;             // 地址阶段按 1 线发送。
  command.AddressSize = QSPI_ADDRESS_32_BITS;            // 当前芯片已进 4 字节地址模式，所以地址宽度是 32 位。
  command.Address = address;                             // 把调用者给的起始地址放进命令里。
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; // 普通读命令不需要交替字节。
  command.DataMode = QSPI_DATA_4_LINES;                   // 数据阶段用 4 线接收。
  command.DummyCycles = 8;                               // qspi_flash 的四输出快速读需要 8 个 dummy 周期。
  command.NbData = length;                               // 一次连续读取 length 字节。
  command.DdrMode = QSPI_DDR_MODE_DISABLE;               // 不启用 DDR 读。
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  // 保持默认的 DDR 半周期配置。
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           // 每次都重新发送 instruction，逻辑最直观。

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {  // 先发起读命令。
    return W25Q256_ERROR;
  }

  if (HAL_QSPI_Receive(&hqspi, buffer, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {  // 再把读出的数据收进用户缓冲区。
    return W25Q256_ERROR;
  }

  return W25Q256_OK;  // 全部接收成功。
}

uint8_t W25Q256_WritePage(uint32_t address, const uint8_t *buffer, uint32_t length)
{
  QSPI_CommandTypeDef command = {0};                // 页编程命令的完整配置结构体。
  uint32_t page_offset = address % W25Q256_PAGE_SIZE; // 当前地址落在页内的偏移，用来检查是否跨页。

  if (length == 0U) {  // 写长度为 0 时不做任何操作，直接返回成功。
    return W25Q256_OK;
  }

  if (buffer == 0 || (address + length) > W25Q256_TOTAL_SIZE) {  // 缓冲区为空或地址越界都不允许继续。
    return W25Q256_INVALID_PARAM;
  }

  if ((page_offset + length) > W25Q256_PAGE_SIZE) {  // 单次页编程不能跨越 256B 页边界。
    return W25Q256_INVALID_PARAM;
  }

  if (W25Q256_Init() != W25Q256_OK) {  // 确保芯片已经初始化完成。
    return W25Q256_ERROR;
  }

  if (W25Q256_WriteEnable() != W25Q256_OK) {  // 每次编程前都必须先置 WREN 位。
    return W25Q256_ERROR;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;     // 页编程指令走 1 线发送。
  command.Instruction = W25Q256_CMD_ADDR4_PAGE_PROGRAM;    // 页编程指令 0x12。
  command.AddressMode = QSPI_ADDRESS_1_LINE;             // 地址阶段按 1 线发出。
  command.AddressSize = QSPI_ADDRESS_32_BITS;            // 地址宽度按 32 位配置。
  command.Address = address;                             // 目标编程地址。
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; // 页编程不需要交替字节。
  command.DataMode = QSPI_DATA_1_LINE;                   // 数据阶段按 1 线发送。
  command.DummyCycles = 0;                               // 页编程命令不需要 dummy 周期。
  command.NbData = length;                               // 本次编程的字节数。
  command.DdrMode = QSPI_DDR_MODE_DISABLE;               // 关闭 DDR。
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  // 维持默认 DDR 半周期配置。
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           // 每次都发送 instruction。

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {  // 先发送页编程命令头。
    return W25Q256_ERROR;
  }

  if (HAL_QSPI_Transmit(&hqspi, (uint8_t *)buffer, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {  // 再把用户数据推送到 Flash。
    return W25Q256_ERROR;
  }

  return W25Q256_WaitWhileBusy(HAL_QPSI_TIMEOUT_DEFAULT_VALUE);  // 编程后轮询 WIP，直到器件空闲。
}

uint8_t W25Q256_Write(uint32_t address, const uint8_t *buffer, uint32_t length)
{
  uint32_t current_address = address;  // 当前正在写入的物理地址，会随着分页写递增。
  uint32_t remaining = length;         // 还剩多少字节没有写完。
  uint32_t chunk_size = 0U;            // 当前这一轮准备写入的分页大小。

  if (length == 0U) {  // 零长度写入直接返回成功。
    return W25Q256_OK;
  }

  if (buffer == 0 || (address + length) > W25Q256_TOTAL_SIZE) {  // 参数非法时直接返回错误码。
    return W25Q256_INVALID_PARAM;
  }

  while (remaining > 0U) {  // 只要还有数据没写完，就持续拆分成页内写操作。
    chunk_size = W25Q256_PAGE_SIZE - (current_address % W25Q256_PAGE_SIZE);  // 先算出当前页还剩多少空间。
    if (chunk_size > remaining) {  // 如果剩余数据比页尾空间还小，就只写剩余数据那么多。
      chunk_size = remaining;
    }

    if (W25Q256_WritePage(current_address, buffer, chunk_size) != W25Q256_OK) {  // 逐页调用底层页编程接口。
      return W25Q256_ERROR;
    }

    current_address += chunk_size;  // 地址前移到下一段待写区域。
    buffer += chunk_size;           // 数据指针同步前移。
    remaining -= chunk_size;        // 剩余待写字节数递减。
  }

  return W25Q256_OK;  // 所有分页写都完成。
}

uint8_t W25Q256_EraseSector(uint32_t address)
{
  QSPI_CommandTypeDef command = {0};  // 扇区擦除命令结构体。

  if ((address >= W25Q256_TOTAL_SIZE) || ((address % W25Q256_ERASE_SIZE) != 0U)) {  // 地址必须在范围内且 4KB 对齐。
    return W25Q256_INVALID_PARAM;
  }

  if (W25Q256_Init() != W25Q256_OK) {  // 擦除前同样先确保芯片已经初始化。
    return W25Q256_ERROR;
  }

  if (W25Q256_WriteEnable() != W25Q256_OK) {  // 擦除操作前也必须先置 WREN 位。
    return W25Q256_ERROR;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;     // 擦除指令走 1 线。
  command.Instruction = W25Q256_CMD_ADDR4_SECTOR_ERASE_4KB; // 4KB 扇区擦除指令 0x21。
  command.AddressMode = QSPI_ADDRESS_1_LINE;             // 地址阶段按 1 线发出。
  command.AddressSize = QSPI_ADDRESS_32_BITS;            // 地址宽度仍然是 32 位。
  command.Address = address;                             // 扇区起始地址。
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; // 擦除命令不需要交替字节。
  command.DataMode = QSPI_DATA_NONE;                     // 擦除命令没有数据阶段。
  command.DummyCycles = 0;                               // 也不需要 dummy 周期。
  command.DdrMode = QSPI_DDR_MODE_DISABLE;               // 不使用 DDR。
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  // 维持默认 DDR 半周期配置。
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           // 每条命令都发送 instruction。

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {  // 发出擦除命令头。
    return W25Q256_ERROR;
  }

  return W25Q256_WaitWhileBusy(W25Q256JV_ERASE_MAX_TIME);  // 擦除时间长，使用单独的最大擦除超时常量。
}

uint8_t W25Q256_WaitWhileBusy(uint32_t timeout_ms)
{
  uint32_t tick_start = HAL_GetTick();  // 记录起始 tick，用于做超时判断。
  uint8_t sr1 = 0U;                     // 暂存读回的状态寄存器 1。

  do {  // 至少读一次状态寄存器，判断当前是否仍在忙。
    if (W25Q256_ReadStatusRegister1(&sr1) != W25Q256_OK) {  // 读状态寄存器失败时直接返回错误。
      return W25Q256_ERROR;
    }

    if ((sr1 & W25Q256JV_SR_WIP) == 0U) {  // WIP 为 0 说明写/擦操作已经结束。
      return W25Q256_OK;
    }
  } while ((HAL_GetTick() - tick_start) <= timeout_ms);  // 只要没超时就继续轮询。

  return W25Q256_BUSY;  // 到达超时仍然忙，返回 busy 状态而不是普通 error。
}

uint8_t W25Q256_GetStatus(void)
{
  uint8_t sr1 = 0U;  // 暂存状态寄存器 1。

  if (W25Q256_ReadStatusRegister1(&sr1) != W25Q256_OK) {  // 先尝试读取 SR1。
    return W25Q256_ERROR;
  }

  return ((sr1 & W25Q256JV_SR_WIP) != 0U) ? W25Q256_BUSY : W25Q256_OK;  // 根据 WIP 位直接对外返回忙或空闲。
}

void W25Q256_GetInfo(W25Q256_Info *info)
{
  if (info == 0) {  // 调用者没给输出结构体时不做任何写入。
    return;
  }

  info->JedecId = W25Q256JV_JEDEC_ID;                           // 固定 JEDEC ID 常量。
  info->FlashSize = W25Q256_TOTAL_SIZE;                         // 总容量 32MB。
  info->EraseSectorSize = W25Q256_ERASE_SIZE;                   // 单次最小擦除粒度 4KB。
  info->EraseSectorsNumber = W25Q256_TOTAL_SIZE / W25Q256_ERASE_SIZE;  // 总扇区数量。
  info->ProgPageSize = W25Q256_PAGE_SIZE;                       // 单次页编程粒度 256B。
  info->ProgPagesNumber = W25Q256_TOTAL_SIZE / W25Q256_PAGE_SIZE;       // 总页数。
}

static uint8_t W25Q256_ResetMemory(void)
{
  QSPI_CommandTypeDef command = {0};  // 复位命令和复位使能命令共用同一个 command 结构。

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;     // 指令按 1 线发。
  command.AddressMode = QSPI_ADDRESS_NONE;               // 本命令不需要地址。
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; // 本命令不需要交替字节。
  command.DataMode = QSPI_DATA_NONE;                     // 本命令没有数据阶段。
  command.DummyCycles = 0;                               // 本命令不需要 dummy cycle。
  command.DdrMode = QSPI_DDR_MODE_DISABLE;               // DDR 模式禁用。
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  // 保持默认 DDR 半周期配置。
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           // 每次命令都带 instruction。

  command.Instruction = W25Q256_CMD_SPECIAL_ENABLE_RESET;  // 先发 enable reset，防止误触发芯片复位。
  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
    return W25Q256_ERROR;
  }

  command.Instruction = W25Q256_CMD_SPECIAL_RESET_DEVICE;  // 再真正发 reset memory 命令。
  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
    return W25Q256_ERROR;
  }

  HAL_Delay(1);  // 复位后给器件一点恢复时间。
  return W25Q256_WaitWhileBusy(HAL_QPSI_TIMEOUT_DEFAULT_VALUE);  // 轮询 busy 位，直到芯片恢复空闲。
}

static uint8_t W25Q256_WriteEnable(void)
{
  QSPI_CommandTypeDef command = {0};  // 写使能命令结构体。
  uint32_t tick_start = HAL_GetTick(); // 用于限制轮询 WREN 位的最大等待时间。
  uint8_t sr1 = 0U;                    // 暂存状态寄存器 1，用来查看 WREN 位。

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;     // 指令阶段按 1 线发。
  command.Instruction = W25Q256_CMD_STD_WRITE_ENABLE;   // 写使能命令 0x06。
  command.AddressMode = QSPI_ADDRESS_NONE;               // 本命令不需要地址。
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; // 本命令不需要交替字节。
  command.DataMode = QSPI_DATA_NONE;                     // 本命令不需要数据阶段。
  command.DummyCycles = 0;                               // 不需要 dummy cycle。
  command.DdrMode = QSPI_DDR_MODE_DISABLE;               // DDR 模式禁用。
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  // 保持默认 DDR 半周期配置。
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           // 每条命令都发送 instruction。

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {  // 先把写使能命令发出去。
    return W25Q256_ERROR;
  }

  while ((HAL_GetTick() - tick_start) <= HAL_QPSI_TIMEOUT_DEFAULT_VALUE) {  // 在超时窗口内轮询确认 WREN 是否生效。
    if (W25Q256_ReadStatusRegister1(&sr1) != W25Q256_OK) {  // 每轮都重新读取 SR1。
      return W25Q256_ERROR;
    }

    if ((sr1 & W25Q256JV_SR_WREN) != 0U) {  // WREN 位置 1，说明后续写/擦命令已经被器件接受。
      return W25Q256_OK;
    }
  }

  return W25Q256_ERROR;  // 到超时仍未置位，视为失败。
}


static uint8_t W25Q256_GlobalBlockUnlock(void)
{
  QSPI_CommandTypeDef command = {0};  // 全局解锁命令结构体。

  if (W25Q256_WriteEnable() != W25Q256_OK) {  // 改保护状态前必须先写使能。
    return W25Q256_ERROR;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;     // 指令按 1 线发。
  command.Instruction = W25Q256_CMD_PROTECT_GLOBAL_BLOCK_UNLOCK;         // 全局块解锁命令。
  command.AddressMode = QSPI_ADDRESS_NONE;               // 不带地址。
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; // 不带交替字节。
  command.DataMode = QSPI_DATA_NONE;                     // 不带数据。
  command.DummyCycles = 0;                               // 不需要 dummy 周期。
  command.DdrMode = QSPI_DDR_MODE_DISABLE;               // DDR 模式禁用。
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  // 保持默认 DDR 半周期配置。
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           // 每条命令都发送 instruction。

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {  // 下发全局解锁命令。
    return W25Q256_ERROR;
  }

  return W25Q256_WaitWhileBusy(HAL_QPSI_TIMEOUT_DEFAULT_VALUE);  // 等待器件完成保护状态更新。
}

static uint8_t W25Q256_WriteStatusRegister1(uint8_t sr1)
{
  QSPI_CommandTypeDef command = {0};  // 写状态寄存器命令结构体。

  if (W25Q256_WriteEnable() != W25Q256_OK) {  // 改状态寄存器前必须先写使能。
    return W25Q256_ERROR;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;     // 指令按 1 线发。
  command.Instruction = W25Q256_CMD_STD_WRITE_STATUS_REG1;            // 单独写状态寄存器 1 命令 0x01。
  command.AddressMode = QSPI_ADDRESS_NONE;               // 该命令不带地址。
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; // 该命令不带交替字节。
  command.DataMode = QSPI_DATA_1_LINE;                   // 数据阶段按 1 线发送。
  command.DummyCycles = 0;                               // 不需要 dummy 周期。
  command.NbData = 1U;                                   // 这里只发送 1 字节 SR1。
  command.DdrMode = QSPI_DDR_MODE_DISABLE;               // DDR 模式禁用。
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  // 保持默认 DDR 半周期配置。
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           // 每条命令都发送 instruction。

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {  // 先发送写 SR1 的命令头。
    return W25Q256_ERROR;
  }

  if (HAL_QSPI_Transmit(&hqspi, &sr1, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {  // 再把 1 字节 SR1 数据发出去。
    return W25Q256_ERROR;
  }

  return W25Q256_WaitWhileBusy(HAL_QPSI_TIMEOUT_DEFAULT_VALUE);  // 等待 SR1 写入完成。
}

static uint8_t W25Q256_WriteStatusRegister2(uint8_t sr2)
{
  QSPI_CommandTypeDef command = {0};  // 写状态寄存器 2 命令结构体。

  if (W25Q256_WriteEnable() != W25Q256_OK) {  // 改 SR2 前同样必须先写使能。
    return W25Q256_ERROR;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;     // 指令按 1 线发。
  command.Instruction = W25Q256_CMD_STATUS_WRITE_REG2;   // 单独写状态寄存器 2 命令 0x31。
  command.AddressMode = QSPI_ADDRESS_NONE;               // 该命令不带地址。
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; // 该命令不带交替字节。
  command.DataMode = QSPI_DATA_1_LINE;                   // 数据阶段按 1 线发送。
  command.DummyCycles = 0;                               // 不需要 dummy 周期。
  command.NbData = 1U;                                   // 这里只发送 1 字节 SR2。
  command.DdrMode = QSPI_DDR_MODE_DISABLE;               // DDR 模式禁用。
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  // 保持默认 DDR 半周期配置。
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           // 每条命令都发送 instruction。

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {  // 先发送写 SR2 的命令头。
    return W25Q256_ERROR;
  }

  if (HAL_QSPI_Transmit(&hqspi, &sr2, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {  // 再把 1 字节 SR2 数据发出去。
    return W25Q256_ERROR;
  }

  return W25Q256_WaitWhileBusy(HAL_QPSI_TIMEOUT_DEFAULT_VALUE);  // 等待 SR2 写入完成。
}

static uint8_t W25Q256_ReadStatusRegister(uint8_t instruction, uint8_t *status)
{
  QSPI_CommandTypeDef command = {0};

  if (status == NULL) {
    return W25Q256_INVALID_PARAM;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = instruction;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0;
  command.NbData = 1;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
    return W25Q256_ERROR;
  }

  if (HAL_QSPI_Receive(&hqspi, status, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
    return W25Q256_ERROR;
  }

  return W25Q256_OK;
}

static uint8_t W25Q256_ReadStatusRegister1(uint8_t *status)
{
  return W25Q256_ReadStatusRegister(W25Q256_CMD_STD_READ_STATUS_REG1, status);
}

static uint8_t W25Q256_ReadStatusRegister2(uint8_t *status)
{
  return W25Q256_ReadStatusRegister(W25Q256_CMD_STATUS_READ_REG2, status);
}
