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
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include "initial.h"
#include "display.h"
#include "initial.h"
#include "usb_storage.h"
#include "usb_file_browser.h"
#include "ota_update.h"
/* USER CODE BEGIN Variables */
osSemaphoreId_t  LvglReadySemHandle;
osEventFlagsId_t mountEventHandle;
osEventFlagsId_t usbEventHandle;
osEventFlagsId_t uiMSGEventHandle;
osEventFlagsId_t otaEventHandle;


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

  LvglReadySemHandle = osSemaphoreNew(1, 0, NULL);
  mountEventHandle   = osEventFlagsNew(NULL);
  usbEventHandle     = osEventFlagsNew(NULL);
  uiMSGEventHandle   = osEventFlagsNew(NULL);
  otaEventHandle     = osEventFlagsNew(NULL);

  /* add threads, ... */
  Initial_TaskHandle  = osThreadNew(Initial_Task, NULL, &Initial_Task_attributes);
  Display_TaskHandle  = osThreadNew(Display_Task, NULL, &Display_Task_attributes);
  USB_TaskHandle      = osThreadNew(USB_mount_Task, NULL, &USB_Task_attributes);
  FileBrow_TaskHandle = osThreadNew(FileBrow_Task, NULL, &FileBrow_Task_attributes);
  OTA_TaskHandle      = osThreadNew(OTA_Update_Task, NULL, &OTA_Task_attributes);
}



