/*
负责检测 USB 存储设备的连接状态，并在设备连接时将其挂载到文件系统中，断开时卸载。
接收来自usb_host.c的事件通知，根据事件不同执行挂载和卸载
挂载和卸载成功后会给lvgl和文件浏览任务发送事件通知，让他们进一步更新界面和文件列表。
*/
#include "usb_storage.h"
#include "event_conf.h"
#include "cmsis_os2.h"
#include "fatfs.h"

void UsbStorage_Init(UsbStorage_t *self,
                     osEventFlagsId_t usb_event,
                     osEventFlagsId_t mount_event)
{
  self->usb_event = usb_event;
  self->mount_event = mount_event;
  self->mounted = 0;
}

void UsbStorage_Task(void *argument)
{
  UsbStorage_t *self = (UsbStorage_t *)argument;
  for (;;) {
        uint32_t flags = osEventFlagsWait(
        self->usb_event,
        USB_EVT_READY | USB_EVT_DISCONNECT,
        osFlagsWaitAny,
        osWaitForever);
        
    if (0U != (flags & USB_EVT_DISCONNECT)) {
      if (self->mounted) {
        f_mount(NULL, USERHPath, 0);
        self->mounted = 0;
        osEventFlagsSet(self->mount_event, 
                        MOUNT_EVT_DISCONNECT);
        
      }
    }

    if (0U != (flags & USB_EVT_READY)) {
      if (!self->mounted) {
        if (f_mount(&USERHFatFS, USERHPath, 1) == FR_OK) {
          self->mounted = 1;
          osEventFlagsSet(self->mount_event, 
                          MOUNT_EVT_READY);
          
        }
      }
    }
  }
}
