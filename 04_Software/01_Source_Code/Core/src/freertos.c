/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"

#include "initial.h"
#include "display.h"
#include "initial.h"
#include "usb_storage.h"
#include "usb_file_browser.h"
#include "ota_update.h"
#include "app_usbhost_port.h"

osSemaphoreId_t  Lvgl_Ready_Sem;
osEventFlagsId_t usb_mount_Event;
osEventFlagsId_t usb_Event;
osEventFlagsId_t ui_MSG_Event;
osEventFlagsId_t ota_download_Event;

osThreadId_t Initial_TaskHandle;
const osThreadAttr_t Initial_Task_attributes = {
  .name = "Initial_Task",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t Display_TaskHandle;
const osThreadAttr_t Display_Task_attributes = {
  .name = "Display_Task",
  .stack_size = 8192 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t USB_TaskHandle;
const osThreadAttr_t USB_Task_attributes = {
  .name = "USB_Task",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t FileBrow_TaskHandle;
const osThreadAttr_t FileBrow_Task_attributes = {
  .name = "FileBrow_Task",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t OTA_TaskHandle;
const osThreadAttr_t OTA_Task_attributes = {
  .name = "OTA_Task",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

void MX_FREERTOS_Init(void) {

  /* 创建信号量和事件标志 */
  Lvgl_Ready_Sem      = osSemaphoreNew(1, 0, NULL);
  usb_mount_Event     = osEventFlagsNew(NULL);
  usb_Event           = osEventFlagsNew(NULL);
  ui_MSG_Event        = osEventFlagsNew(NULL);
  ota_download_Event  = osEventFlagsNew(NULL);

  /* 依赖注入 */
  static UsbStorage_t s_usb_storage;
  UsbStorage_Init(&s_usb_storage, 
                  usb_Event, 
                  usb_mount_Event);

  static UsbFileBrowser_t s_file_brow_ctx;
  UsbFileBrowser_Init(&s_file_brow_ctx, 
                      usb_mount_Event, 
                      ui_MSG_Event);
  
  static Display_Task_Ctx_t s_display_ctx;
  display_Init(&s_display_ctx,
            Lvgl_Ready_Sem,
            ui_MSG_Event,
            ota_download_Event,
            &s_file_brow_ctx);

  app_usb_host_port_init(usb_Event);

  /* 创建线程 */
  Initial_TaskHandle  = osThreadNew(Initial_Task, 
                                    NULL, 
                                    &Initial_Task_attributes);
  Display_TaskHandle  = osThreadNew(Display_Task, 
                                    &s_display_ctx, 
                                    &Display_Task_attributes);
  USB_TaskHandle      = osThreadNew(UsbStorage_Task, 
                                    &s_usb_storage, 
                                    &USB_Task_attributes);
  FileBrow_TaskHandle = osThreadNew(UsbFileBrowser_Task, 
                                    &s_file_brow_ctx, 
                                    &FileBrow_Task_attributes);
  OTA_TaskHandle      = osThreadNew(OTA_Update_Task, 
                                    NULL, 
                                    &OTA_Task_attributes);
}



