#ifndef DIS_HT05_H
#define DIS_HT05_H

#include "stm32h7xx.h"

// 当前工程显示链路已经固定为 5 寸 800x480 RGB565。
// 这里只保留 LVGL 双缓冲切屏真正需要的最小接口，不再维护旧 BSP 的多屏和绘图杂项。

#define LCD_FB0_ADDRESS      0xD0000000U
#define LCD_FB1_ADDRESS      0xD00BB800U
#define LCD_FB_SIZE          (800U * 480U * 2U)
#define LCD_BYTES_PER_PIXEL  2U

#define LCD_PIXEL_WIDTH      800U
#define LCD_PIXEL_HEIGHT     480U

uint32_t LCD_GetActiveBufferAddress(void);
void LCD_SetActiveBufferAddress(uint32_t fb_address);
void lcd_init(void);

#endif