#ifndef USB_FILE_BROWSER_H
#define USB_FILE_BROWSER_H

#include <stdint.h>

#define USB_VIEW_MAX_ITEMS    18
#define USB_VIEW_NAME_MAX     64

typedef struct
{
    char name[USB_VIEW_NAME_MAX];
    uint8_t is_dir;   // 1 = 文件夹，0 = 文件
} UsbViewItem_t;

typedef struct
{
    char path[16];    // 先只显示 0:/
    uint8_t count;
    UsbViewItem_t items[USB_VIEW_MAX_ITEMS];
} UsbViewModel_t;

extern UsbViewModel_t g_usb_view_model;

void FileBrow_Task(void *argument);

#endif /* USB_FILE_BROWSER_H */ 