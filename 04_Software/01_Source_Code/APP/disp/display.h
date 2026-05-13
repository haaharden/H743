#ifndef DISPLAY_H
#define DISPLAY_H

#include "cmsis_os2.h"
#include "usb_file_browser.h"

typedef struct {
    osSemaphoreId_t  lv_ready_Sem;
    osEventFlagsId_t ui_msg_event;
    osEventFlagsId_t ota_event;
    UsbFileBrowser_t *usb_file_browser;
} Display_Task_Ctx_t;

void display_Init(Display_Task_Ctx_t *self,
               osSemaphoreId_t lv_ready_Sem,
               osEventFlagsId_t ui_msg_event,
               osEventFlagsId_t ota_event,
               UsbFileBrowser_t *usb_file_browser);
void Display_Task(void *argument);

#endif /* DISPLAY_H */