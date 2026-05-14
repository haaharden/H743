/*
负责检测 USB 存储设备的连接状态，并在设备连接时将其挂载到文件系统中，断开时卸载。
接收来自usb_host.c的事件通知，根据事件不同执行挂载和卸载
挂载和卸载成功后会给lvgl和文件浏览任务发送事件通知，让他们进一步更新界面和文件列表。
*/
#include "app_usb_storage.h"
#include "event_conf.h"
#include "cmsis_os2.h"
#include "fatfs.h"

void UsbStorage_Init(usb_storage_ctx *self,
                     osEventFlagsId_t usb_event,
                     osEventFlagsId_t mount_event)
{
  self->usb_event = usb_event;
  self->mount_event = mount_event;
  self->mounted = 0;
}

void UsbStorage_Task(void *argument)
{
  usb_storage_ctx *ctx = (usb_storage_ctx *)argument;
  for (;;) {
        uint32_t flags = osEventFlagsWait(
        ctx ->usb_event,
        USB_EVT_READY | USB_EVT_DISCONNECT,
        osFlagsWaitAny,
        osWaitForever);
        
    if (0U != (flags & USB_EVT_DISCONNECT)) {
      if (ctx->mounted) {
        f_mount(NULL, USERHPath, 0);
        ctx->mounted = 0;
        osEventFlagsSet(ctx->mount_event, 
                        MOUNT_EVT_DISCONNECT);
        
      }
    }

    if (0U != (flags & USB_EVT_READY)) {
      if (!ctx->mounted) {
        if (f_mount(&USERHFatFS, USERHPath, 1) == FR_OK) {
          ctx->mounted = 1;
          osEventFlagsSet(ctx->mount_event, 
                          MOUNT_EVT_READY);
          
        }
      }
    }
  }
}
