/*
负责检测 USB 存储设备的连接状态，并在设备连接时将其挂载到文件系统中，断开时卸载。
接收来自usb_host.c的事件通知，根据事件不同执行挂载和卸载
挂载和卸载成功后会给lvgl和文件浏览任务发送事件通知，让他们进一步更新界面和文件列表。
*/
#include "usb_storage.h"
#include "event_conf.h"
#include "cmsis_os2.h"
#include "fatfs.h"

extern osEventFlagsId_t usbEventHandle;
extern osEventFlagsId_t mountEventHandle;

void USB_mount_Task(void *argument)
{
  static uint8_t mounted = 0;
  for (;;) {
        uint32_t flags = osEventFlagsWait(
        usbEventHandle,
        USB_EVT_READY | USB_EVT_DISCONNECT,
        osFlagsWaitAny,
        osWaitForever);
        
    if (0U != (flags & USB_EVT_DISCONNECT)) {
      if (mounted) {
        f_mount(NULL, USERHPath, 0);
        mounted = 0;
        osEventFlagsSet(mountEventHandle,MOUNT_EVT_DISCONNECT);
        
      }
    }

    if (0U != (flags & USB_EVT_READY)) {
      if (!mounted) {
        if (f_mount(&USERHFatFS, USERHPath, 1) == FR_OK) {
          mounted = 1;
          osEventFlagsSet(mountEventHandle, MOUNT_EVT_READY);
          
        }
      }
    }
  }
}
