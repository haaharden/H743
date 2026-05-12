#include "sdram_w9825g6.h"

#define BSP_SDRAM_TIMEOUT            0xFFFFU
#define BSP_SDRAM_REFRESH_COUNT      824U
#define BSP_SDRAM_TARGET_BANK        FMC_SDRAM_CMD_TARGET_BANK2

// Mode Register: BL=1, Sequential, CAS=3, Standard, Single write burst.
#define BSP_SDRAM_MODEREG_BL_1       0x0000U
#define BSP_SDRAM_MODEREG_BT_SEQ     0x0000U
#define BSP_SDRAM_MODEREG_CAS_3      0x0030U
#define BSP_SDRAM_MODEREG_OM_STD     0x0000U
#define BSP_SDRAM_MODEREG_WB_SINGLE  0x0200U

static uint8_t s_sdram_initialized;

HAL_StatusTypeDef BSP_SDRAM_Init(void)
{
  FMC_SDRAM_CommandTypeDef command = {0};
  HAL_StatusTypeDef status;
  uint32_t mode_register;

  // main 里已经先调用过 MX_FMC_Init，这里只补 SDRAM 芯片启动命令序列。
  if (s_sdram_initialized != 0U) {
    return HAL_OK;
  }

  if (hsdram2.Instance != FMC_SDRAM_DEVICE) {
    return HAL_ERROR;
  }

  command.CommandTarget = BSP_SDRAM_TARGET_BANK;
  command.AutoRefreshNumber = 1U;
  command.ModeRegisterDefinition = 0U;

  // Step1: 打开 SDRAM 时钟。
  command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
  status = HAL_SDRAM_SendCommand(&hsdram2, &command, BSP_SDRAM_TIMEOUT);
  if (status != HAL_OK) {
    return status;
  }

  // Step2: 等待时钟稳定，1ms 远大于手册要求的 100us。
  HAL_Delay(1);

  // Step3: 预充电所有 bank。
  command.CommandMode = FMC_SDRAM_CMD_PALL;
  status = HAL_SDRAM_SendCommand(&hsdram2, &command, BSP_SDRAM_TIMEOUT);
  if (status != HAL_OK) {
    return status;
  }

  // Step4: 执行 8 次自动刷新。
  command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  command.AutoRefreshNumber = 8U;
  status = HAL_SDRAM_SendCommand(&hsdram2, &command, BSP_SDRAM_TIMEOUT);
  if (status != HAL_OK) {
    return status;
  }

  // Step5: 装载模式寄存器。
  mode_register = BSP_SDRAM_MODEREG_BL_1 |
                  BSP_SDRAM_MODEREG_BT_SEQ |
                  BSP_SDRAM_MODEREG_CAS_3 |
                  BSP_SDRAM_MODEREG_OM_STD |
                  BSP_SDRAM_MODEREG_WB_SINGLE;
  command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
  command.AutoRefreshNumber = 1U;
  command.ModeRegisterDefinition = mode_register;
  status = HAL_SDRAM_SendCommand(&hsdram2, &command, BSP_SDRAM_TIMEOUT);
  if (status != HAL_OK) {
    return status;
  }

  // Step6: 配置刷新计数器。64ms / 8192 行，对应当前 FMC 时钟下取 824。
  status = HAL_SDRAM_ProgramRefreshRate(&hsdram2, BSP_SDRAM_REFRESH_COUNT);
  if (status != HAL_OK) {
    return status;
  }

  s_sdram_initialized = 1U;
  return HAL_OK;
}