/*
 *给freertos提供任务入口，以及要传入的上下文
 *给ui提供获取usb文件浏览数据的接口,还有数据类型定义
 */
#ifndef USB_FILE_BROWSER_H
#define USB_FILE_BROWSER_H

#include <stdint.h>
#include "cmsis_os2.h"

#define USB_VIEW_MAX_ITEMS    18
#define USB_VIEW_NAME_MAX     64

typedef struct {
    char name[USB_VIEW_NAME_MAX];
    uint8_t is_dir;
} UsbViewItem_t;

typedef struct {
    char path[16];
    uint8_t count;
    UsbViewItem_t items[USB_VIEW_MAX_ITEMS];
} UsbViewModel_t;

typedef struct {
  osEventFlagsId_t mount_event;
  osEventFlagsId_t ui_msg_event;
  UsbViewModel_t   view_model;    // 当前显示的文件列表数据
} UsbFileBrowser_t;

void UsbFileBrowser_Init(UsbFileBrowser_t *self,
                         osEventFlagsId_t mount_event,
                         osEventFlagsId_t ui_msg_event);
void UsbFileBrowser_Deinit(UsbFileBrowser_t *self);
void UsbFileBrowser_Task(void *argument);
const UsbViewModel_t *UsbFileBrowser_GetViewModel(const UsbFileBrowser_t *self);


#endif
