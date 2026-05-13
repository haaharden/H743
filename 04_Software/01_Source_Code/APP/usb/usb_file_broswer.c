/*
 *接收来自usb_storage.c的事件通知，在USB设备连接时列出根目录下的文件和文件夹，并在USB设备断开时清空列表。
 *当USB设备连接或断开时，通过事件通知UI更新显示。
 */
#include <string.h>

#include "event_conf.h"
#include "ff.h"
#include "log.h"
#include "usb_file_browser.h"

#define USB_FILE_BROWSER_OK      0
#define USB_FILE_BROWSER_ERROR  -1

#define USB_FILE_ATTR_VOLUME  0x08U //过滤掉卷标，ff.c中定义的 AM_VOL 是 0x08

static int UsbFileBrowser_OpenPath(UsbFileBrowser_t *self, const char *path);

void UsbFileBrowser_Init(UsbFileBrowser_t *self,
                         osEventFlagsId_t mount_event,
                         osEventFlagsId_t ui_msg_event)
{
    self->mount_event = mount_event;
    self->ui_msg_event = ui_msg_event;
    memset(&self->view_model, 0, sizeof(self->view_model));
}

void UsbFileBrowser_Deinit(UsbFileBrowser_t *self)
{
    if (self == NULL)
    {
        return;
    }
    memset(self, 0, sizeof(*self));
    self = NULL;
}

// 这里放检测 USB 存储设备的连接状态，列出文件等。
void UsbFileBrowser_Task(void *argument)
{
    UsbFileBrowser_t *self = (UsbFileBrowser_t *)argument;
    for(;;) {
        uint32_t flags = osEventFlagsWait(self->mount_event, 
                                MOUNT_EVT_READY | MOUNT_EVT_DISCONNECT, 
                                osFlagsWaitAny, 
                                osWaitForever);
        if(flags & MOUNT_EVT_READY) {
            if(USB_FILE_BROWSER_OK == UsbFileBrowser_OpenPath(self, "0:/")) {
                osEventFlagsSet(self->ui_msg_event, UI_MSG_EVT_UPDATE);
            }
        }
        if(flags & MOUNT_EVT_DISCONNECT) {
            memset(&self->view_model, 0, sizeof(self->view_model));
            osEventFlagsSet(self->ui_msg_event, UI_MSG_EVT_DISC);
        }
    }
}

const UsbViewModel_t* UsbFileBrowser_GetViewModel(const UsbFileBrowser_t *self)
{
    if (self == NULL) {
    return NULL;
    }
    return &self->view_model;
}

// 扫描指定路径下的文件和文件夹，结果保存在 view_model 中，供界面显示使用。
static int UsbFileBrowser_OpenPath(UsbFileBrowser_t *self, const char *path)
{
    DIR dir;
    FILINFO fno;
    FRESULT fr;
    uint8_t count = 0;

    memset(&self->view_model, 0, sizeof(self->view_model));
    strcpy(self->view_model.path, path);

    fr = f_opendir(&dir, path);
    if (fr != FR_OK)
    {
        return USB_FILE_BROWSER_ERROR;
    }

    while (1)
    {
        fr = f_readdir(&dir, &fno);

        if (fr != FR_OK)
        {
            break;
        }

        // 读完了
        if (fno.fname[0] == '\0')
        {
            break;
        }

        // 最多显示 18 个
        if (count >= USB_VIEW_MAX_ITEMS)
        {
            break;
        }

        // 跳过 . 和 ..
        if (strcmp(fno.fname, ".") == 0 || strcmp(fno.fname, "..") == 0)
        {
            continue;
        }

        if ((fno.fattrib & (AM_HID | AM_SYS | USB_FILE_ATTR_VOLUME)) != 0) {
            continue;
        }

        strncpy(self->view_model.items[count].name,
                fno.fname,
                USB_VIEW_NAME_MAX - 1);

        if (fno.fattrib & AM_DIR)
        {
            self->view_model.items[count].is_dir = 1;
        }
        else
        {
            self->view_model.items[count].is_dir = 0;
        }

        count++;
    }

    f_closedir(&dir);

    self->view_model.count = count;

    return USB_FILE_BROWSER_OK;
}