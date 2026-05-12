#include "disp_ht05.h"

#include <string.h>

#include "gpio.h"
#include "ltdc.h"

#define LCD_ACTIVE_LAYER 0U

static uint32_t g_front_fb_addr;

static uint8_t LCD_IsKnownFramebuffer(uint32_t fb_address)
{
  if (fb_address == LCD_FB0_ADDRESS) {
    return 1U;
  }

  if (fb_address == LCD_FB1_ADDRESS) {
    return 1U;
  }

  return 0U;
}

static void LCD_ClearFramebuffer(uint32_t fb_address)
{
  // 当前初始化只需要清黑屏，RGB565 的黑色全 0，直接 memset 即可。
  memset((void *)fb_address, 0, LCD_FB_SIZE);
}

static void LCD_DisplayOn(void)
{
  __HAL_LTDC_ENABLE(&hltdc);
  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
}

uint32_t LCD_GetActiveBufferAddress(void)
{
  return g_front_fb_addr;
}

void LCD_SetActiveBufferAddress(uint32_t fb_address)
{
  if (LCD_IsKnownFramebuffer(fb_address) == 0U) {
    return;
  }

  g_front_fb_addr = fb_address;
  HAL_LTDC_SetAddress(&hltdc, fb_address, LCD_ACTIVE_LAYER);
}

void lcd_init(void)
{
  // LTDC/GPIO/FMC 都继续复用 CubeMX 生成代码，这里只做 5 寸屏最小显示服务收口。
  if (hltdc.Instance != LTDC) {
    MX_LTDC_Init();
  }

  g_front_fb_addr = hltdc.LayerCfg[LCD_ACTIVE_LAYER].FBStartAdress;
  if (LCD_IsKnownFramebuffer(g_front_fb_addr) == 0U) {
    g_front_fb_addr = LCD_FB0_ADDRESS;
  }

  LCD_ClearFramebuffer(LCD_FB0_ADDRESS);
  LCD_ClearFramebuffer(LCD_FB1_ADDRESS);
  HAL_LTDC_SetAddress(&hltdc, g_front_fb_addr, LCD_ACTIVE_LAYER);
  LCD_DisplayOn();
}