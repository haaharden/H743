#ifndef APP_USB_STORAGE_H
#define APP_USB_STORAGE_H

#include "cmsis_os2.h"

typedef struct {
  osEventFlagsId_t usb_event;
  osEventFlagsId_t mount_event;
  uint8_t mounted;
} usb_storage_ctx;

void UsbStorage_Init(usb_storage_ctx *self,
                     osEventFlagsId_t usb_event,
                     osEventFlagsId_t mount_event);

void UsbStorage_Task(void *argument);

#endif /* APP_USB_STORAGE_H */