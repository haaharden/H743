/*
接收来自usb_storage.c的事件通知，在USB设备连接时列出根目录下的文件和文件夹，并在USB设备断开时清空列表。
*/
#include "usb_file_browser.h"
#include "event_conf.h"
#include "cmsis_os2.h"
#include "ff.h"
#include "log.h"
#include "string.h"

extern osEventFlagsId_t mountEventHandle;
extern osEventFlagsId_t uiMSGEventHandle;
//extern osEventFlagsId_t otaEventHandle;

UsbViewModel_t g_usb_view_model;

#define USB_FILE_ATTR_VOLUME  0x08U //过滤掉卷标，ff.c中定义的 AM_VOL 是 0x08

// 扫描指定路径下的文件和文件夹，结果保存在全局变量 g_usb_view_model 中，供界面显示使用。
static int UsbView_ScanPath(const char *path)
{
    DIR dir;
    FILINFO fno;
    FRESULT fr;
    uint8_t count = 0;

    memset(&g_usb_view_model, 0, sizeof(g_usb_view_model));
    strcpy(g_usb_view_model.path, path);

    fr = f_opendir(&dir, path);
    if (fr != FR_OK)
    {
        return -1;
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

        // //直接通知ota升级，仅供测试使用，后续会改成点击文件才升级
        // if ((fno.fattrib & AM_DIR) == 0)
        // {
        //     if ((strcmp(fno.fname, "app.bin") == 0) ||
        //         (strcmp(fno.fname, "APP.BIN") == 0))
        //     {
        //         osEventFlagsSet(otaEventHandle, OTA_MSG_EVT_START);
        //     }
        // }

        strncpy(g_usb_view_model.items[count].name,
                fno.fname,
                USB_VIEW_NAME_MAX - 1);

        if (fno.fattrib & AM_DIR)
        {
            g_usb_view_model.items[count].is_dir = 1;
        }
        else
        {
            g_usb_view_model.items[count].is_dir = 0;
        }

        count++;
    }

    f_closedir(&dir);

    g_usb_view_model.count = count;

    return 0;
}

void FileBrow_Task(void *argument)
{
    // 这里放检测 USB 存储设备的连接状态，列出文件等。
    for(;;) {
        uint32_t flags = osEventFlagsWait(mountEventHandle, 
                                MOUNT_EVT_READY | MOUNT_EVT_DISCONNECT, 
                                osFlagsWaitAny, 
                                osWaitForever);
        if(flags & MOUNT_EVT_READY) {
            if(UsbView_ScanPath("0:/") == 0) {
                osEventFlagsSet(uiMSGEventHandle, UI_MSG_EVT_UPDATE);
            }
        }
        if(flags & MOUNT_EVT_DISCONNECT) {
            memset(&g_usb_view_model, 0, sizeof(g_usb_view_model));
            osEventFlagsSet(uiMSGEventHandle, UI_MSG_EVT_DISC);
        }
    }
}