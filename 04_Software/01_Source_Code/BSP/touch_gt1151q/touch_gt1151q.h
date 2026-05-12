#ifndef TOUCH_GT1151Q_H
#define TOUCH_GT1151Q_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

// 当前项目按 5 寸屏收口，触摸芯片固定为 GT1151QM。
#define TOUCH_I2C_ADDRESS         0x28U
#define TOUCH_MAX_WIDTH           800U
#define TOUCH_MAX_HEIGHT          480U

// 推荐直接用这组接口做后续接入。
HAL_StatusTypeDef Touch_Init(void);
uint8_t Touch_ReadPoint(uint16_t *x, uint16_t *y);
uint8_t Touch_HasEventPending(void);
void Touch_ClearEventPending(void);

// 下面这组是从旧 touch 目录保留下来的兼容接口，后面你自己接入时不用再翻旧名字。
void I2C_Touch_Init(void);
uint32_t I2C_WriteBytes(uint8_t clientAddr, uint8_t *pBuffer, uint8_t numByteToWrite);
uint32_t I2C_ReadBytes(uint8_t clientAddr, uint8_t *pBuffer, uint16_t numByteToRead);
void I2C_ResetChip(void);
void I2C_GTP_IRQDisable(void);
void I2C_GTP_IRQEnable(void);

int8_t GTP_Reset_Guitar(void);
int32_t GTP_Read_Version(void);
void GTP_IRQ_Disable(void);
void GTP_IRQ_Enable(void);
int32_t GTP_Init_Panel(void);
int8_t GTP_Send_Command(uint8_t command);
int GTP_Execu(int *x, int *y);
uint8_t GTP_HasEventPending(void);
void GTP_ClearEventPending(void);

#ifdef __cplusplus
}
#endif

#endif