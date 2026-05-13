#include <stdio.h>
#include <string.h>

#include "fatfs.h"
#include "usb_host.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_port_flash_fs.h"
#include "lvgl.h"
#include "cmsis_os2.h"

#include "initial.h"
#include "sdram_w9825g6.h"
#include "flash_w25q256.h"
#include "log.h"

static void Initial(void)
{
  BSP_SDRAM_Init();
  W25Q256_Init();
  MX_FATFS_Init();
  MX_USB_HOST_Init();

  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();
  lv_port_flash_fs_init();
  LOGI("Init successful.");
}

extern osSemaphoreId_t Lvgl_Ready_Sem;
void Initial_Task(void *argument)
{
  Initial();
  osSemaphoreRelease(Lvgl_Ready_Sem);
  osThreadExit();
}