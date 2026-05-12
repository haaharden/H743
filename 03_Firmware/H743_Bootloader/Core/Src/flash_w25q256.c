#include "flash_w25q256.h"

#include <string.h>

static uint8_t g_w25q256_ready = 0U;

static uint8_t W25Q256_ResetMemory(void);
static uint8_t W25Q256_WriteEnable(void);
static uint8_t W25Q256_GlobalBlockUnlock(void);
static uint8_t W25Q256_WriteStatusRegister1(uint8_t sr1);
static uint8_t W25Q256_WriteStatusRegister2(uint8_t sr2);
static uint8_t W25Q256_ReadStatusRegister1(uint8_t *status);
static uint8_t W25Q256_ReadStatusRegister2(uint8_t *status);

uint8_t W25Q256_Init(void)
{
  uint8_t sr1 = 0U;
  uint8_t sr2 = 0U;

  if (g_w25q256_ready != 0U)
  {
    return W25Q256_OK;
  }

  if (W25Q256_ResetMemory() != W25Q256_OK)
  {
    return W25Q256_NOT_SUPPORTED;
  }

  if (W25Q256_GlobalBlockUnlock() != W25Q256_OK)
  {
    return W25Q256_ERROR;
  }

  if (W25Q256_WriteStatusRegister1(0x00U) != W25Q256_OK)
  {
    return W25Q256_ERROR;
  }

  if (W25Q256_WriteStatusRegister2(W25Q256_SR2_QE_MASK) != W25Q256_OK)
  {
    return W25Q256_ERROR;
  }

  if (W25Q256_ReadStatusRegister1(&sr1) != W25Q256_OK)
  {
    return W25Q256_ERROR;
  }

  if ((sr1 & W25Q256JV_SR_BP_MASK) != 0U)
  {
    return W25Q256_ERROR;
  }

  if (W25Q256_ReadStatusRegister2(&sr2) != W25Q256_OK)
  {
    return W25Q256_ERROR;
  }

  if ((sr2 & W25Q256_SR2_QE_MASK) == 0U)
  {
    return W25Q256_ERROR;
  }

  g_w25q256_ready = 1U;
  return W25Q256_OK;
}

uint8_t W25Q256_ReadJedecId(uint32_t *jedec_id)
{
  QSPI_CommandTypeDef command = {0};
  uint8_t data[3] = {0};

  if (jedec_id == 0)
  {
    return W25Q256_INVALID_PARAM;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = W25Q256_CMD_STD_READ_JEDEC_ID;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0;
  command.NbData = sizeof(data);
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  if (HAL_QSPI_Receive(&hqspi, data, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  *jedec_id = (((uint32_t)data[0]) << 16) | (((uint32_t)data[1]) << 8) | data[2];
  return W25Q256_OK;
}

uint8_t W25Q256_Read(uint32_t address, uint8_t *buffer, uint32_t length)
{
  QSPI_CommandTypeDef command = {0};

  if (length == 0U)
  {
    return W25Q256_OK;
  }

  if ((buffer == 0) || ((address + length) > W25Q256_TOTAL_SIZE))
  {
    return W25Q256_INVALID_PARAM;
  }

  if (W25Q256_Init() != W25Q256_OK)
  {
    return W25Q256_ERROR;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = W25Q256_CMD_QUAD_FAST_READ_OUTPUT_4BYTE;
  command.AddressMode = QSPI_ADDRESS_1_LINE;
  command.AddressSize = QSPI_ADDRESS_32_BITS;
  command.Address = address;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_4_LINES;
  command.DummyCycles = 8;
  command.NbData = length;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  if (HAL_QSPI_Receive(&hqspi, buffer, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  return W25Q256_OK;
}

uint8_t W25Q256_WritePage(uint32_t address, const uint8_t *buffer, uint32_t length)
{
  QSPI_CommandTypeDef command = {0};
  uint32_t page_offset = address % W25Q256_PAGE_SIZE;

  if (length == 0U)
  {
    return W25Q256_OK;
  }

  if ((buffer == 0) || ((address + length) > W25Q256_TOTAL_SIZE))
  {
    return W25Q256_INVALID_PARAM;
  }

  if ((page_offset + length) > W25Q256_PAGE_SIZE)
  {
    return W25Q256_INVALID_PARAM;
  }

  if (W25Q256_Init() != W25Q256_OK)
  {
    return W25Q256_ERROR;
  }

  if (W25Q256_WriteEnable() != W25Q256_OK)
  {
    return W25Q256_ERROR;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = W25Q256_CMD_ADDR4_PAGE_PROGRAM;
  command.AddressMode = QSPI_ADDRESS_1_LINE;
  command.AddressSize = QSPI_ADDRESS_32_BITS;
  command.Address = address;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0;
  command.NbData = length;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  if (HAL_QSPI_Transmit(&hqspi, (uint8_t *)buffer, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  return W25Q256_WaitWhileBusy(HAL_QPSI_TIMEOUT_DEFAULT_VALUE);
}

uint8_t W25Q256_Write(uint32_t address, const uint8_t *buffer, uint32_t length)
{
  uint32_t current_address = address;
  uint32_t remaining = length;
  uint32_t chunk_size = 0U;

  if (length == 0U)
  {
    return W25Q256_OK;
  }

  if ((buffer == 0) || ((address + length) > W25Q256_TOTAL_SIZE))
  {
    return W25Q256_INVALID_PARAM;
  }

  while (remaining > 0U)
  {
    chunk_size = W25Q256_PAGE_SIZE - (current_address % W25Q256_PAGE_SIZE);
    if (chunk_size > remaining)
    {
      chunk_size = remaining;
    }

    if (W25Q256_WritePage(current_address, buffer, chunk_size) != W25Q256_OK)
    {
      return W25Q256_ERROR;
    }

    current_address += chunk_size;
    buffer += chunk_size;
    remaining -= chunk_size;
  }

  return W25Q256_OK;
}

uint8_t W25Q256_EraseSector(uint32_t address)
{
  QSPI_CommandTypeDef command = {0};

  if ((address >= W25Q256_TOTAL_SIZE) || ((address % W25Q256_ERASE_SIZE) != 0U))
  {
    return W25Q256_INVALID_PARAM;
  }

  if (W25Q256_Init() != W25Q256_OK)
  {
    return W25Q256_ERROR;
  }

  if (W25Q256_WriteEnable() != W25Q256_OK)
  {
    return W25Q256_ERROR;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = W25Q256_CMD_ADDR4_SECTOR_ERASE_4KB;
  command.AddressMode = QSPI_ADDRESS_1_LINE;
  command.AddressSize = QSPI_ADDRESS_32_BITS;
  command.Address = address;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_NONE;
  command.DummyCycles = 0;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  return W25Q256_WaitWhileBusy(W25Q256JV_ERASE_MAX_TIME);
}

uint8_t W25Q256_WaitWhileBusy(uint32_t timeout_ms)
{
  uint32_t tick_start = HAL_GetTick();
  uint8_t sr1 = 0U;

  do
  {
    if (W25Q256_ReadStatusRegister1(&sr1) != W25Q256_OK)
    {
      return W25Q256_ERROR;
    }

    if ((sr1 & W25Q256JV_SR_WIP) == 0U)
    {
      return W25Q256_OK;
    }
  } while ((HAL_GetTick() - tick_start) <= timeout_ms);

  return W25Q256_BUSY;
}

static uint8_t W25Q256_ResetMemory(void)
{
  QSPI_CommandTypeDef command = {0};

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_NONE;
  command.DummyCycles = 0;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  command.Instruction = W25Q256_CMD_SPECIAL_ENABLE_RESET;
  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  command.Instruction = W25Q256_CMD_SPECIAL_RESET_DEVICE;
  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  HAL_Delay(1U);

  command.Instruction = W25Q256_CMD_ADDR4_ENTER_MODE;
  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  return W25Q256_OK;
}

static uint8_t W25Q256_WriteEnable(void)
{
  QSPI_CommandTypeDef command = {0};
  uint8_t sr1 = 0U;

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = W25Q256_CMD_STD_WRITE_ENABLE;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_NONE;
  command.DummyCycles = 0;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  if (W25Q256_ReadStatusRegister1(&sr1) != W25Q256_OK)
  {
    return W25Q256_ERROR;
  }

  return ((sr1 & W25Q256JV_SR_WREN) != 0U) ? W25Q256_OK : W25Q256_ERROR;
}

static uint8_t W25Q256_GlobalBlockUnlock(void)
{
  QSPI_CommandTypeDef command = {0};

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = W25Q256_CMD_PROTECT_GLOBAL_BLOCK_UNLOCK;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_NONE;
  command.DummyCycles = 0;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  return W25Q256_OK;
}

static uint8_t W25Q256_ReadStatusRegister1(uint8_t *status)
{
  QSPI_CommandTypeDef command = {0};

  if (status == NULL)
  {
    return W25Q256_INVALID_PARAM;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = W25Q256_CMD_STD_READ_STATUS_REG1;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0;
  command.NbData = 1U;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  if (HAL_QSPI_Receive(&hqspi, status, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  return W25Q256_OK;
}

static uint8_t W25Q256_ReadStatusRegister2(uint8_t *status)
{
  QSPI_CommandTypeDef command = {0};

  if (status == NULL)
  {
    return W25Q256_INVALID_PARAM;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = W25Q256_CMD_STATUS_READ_REG2;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0;
  command.NbData = 1U;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  if (HAL_QSPI_Receive(&hqspi, status, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  return W25Q256_OK;
}

static uint8_t W25Q256_WriteStatusRegister1(uint8_t sr1)
{
  QSPI_CommandTypeDef command = {0};

  if (W25Q256_WriteEnable() != W25Q256_OK)
  {
    return W25Q256_ERROR;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = W25Q256_CMD_STD_WRITE_STATUS_REG1;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0;
  command.NbData = 1U;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  if (HAL_QSPI_Transmit(&hqspi, &sr1, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  return W25Q256_WaitWhileBusy(HAL_QPSI_TIMEOUT_DEFAULT_VALUE);
}

static uint8_t W25Q256_WriteStatusRegister2(uint8_t sr2)
{
  QSPI_CommandTypeDef command = {0};

  if (W25Q256_WriteEnable() != W25Q256_OK)
  {
    return W25Q256_ERROR;
  }

  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = W25Q256_CMD_STATUS_WRITE_REG2;
  command.AddressMode = QSPI_ADDRESS_NONE;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = QSPI_DATA_1_LINE;
  command.DummyCycles = 0;
  command.NbData = 1U;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  if (HAL_QSPI_Transmit(&hqspi, &sr2, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return W25Q256_ERROR;
  }

  return W25Q256_WaitWhileBusy(HAL_QPSI_TIMEOUT_DEFAULT_VALUE);
}