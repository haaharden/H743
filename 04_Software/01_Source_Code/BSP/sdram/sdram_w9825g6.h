#ifndef SDRAM_W9825G6_H
#define SDRAM_W9825G6_H

#include "fmc.h"

// 当前工程只需要把已经由 CubeMX 配好的 FMC-SDRAM 控制器真正带到可工作的状态。
// GPIO、FMC 时序参数继续复用 Src/fmc.c 里的 hsdram2，不在这里重复维护。

#define SDRAM_BASE_ADDRESS 0xD0000000UL
#define SDRAM_SIZE_BYTES   0x04000000UL

HAL_StatusTypeDef BSP_SDRAM_Init(void);

#endif