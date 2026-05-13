#ifndef USB_STORAGE_H
#define USB_STORAGE_H

typedef struct {
  osEventFlagsId_t usb_event;
  osEventFlagsId_t mount_event;
  uint8_t mounted;
} UsbStorage_t;

void UsbStorage_Init(UsbStorage_t *self,
                     osEventFlagsId_t usb_event,
                     osEventFlagsId_t mount_event);

void UsbStorage_Task(void *argument);

#endif /* USB_STORAGE_H */