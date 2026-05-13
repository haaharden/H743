/*设置usb相关的事件组成员定义*/
#ifndef EVENT_CONF_H
#define EVENT_CONF_H

//usb插拔事件组
#define USB_EVT_READY        (1UL << 0)   /* USB 设备连接 */
#define USB_EVT_DISCONNECT   (1UL << 1)   /* USB 设备断开 */

//挂载消息事件组
#define MOUNT_EVT_IDLE         (1UL << 0)   /* USB 设备空闲 */
#define MOUNT_EVT_READY        (1UL << 1)   /* USB 设备连接，文件系统挂载成功 */
#define MOUNT_EVT_DISCONNECT   (1UL << 2)   /* USB 设备断开 */

//UI 消息事件组
#define UI_MSG_EVT_DISC        (1UL << 0)   /* USB 设备断开消息事件 */
#define UI_MSG_EVT_UPDATE      (1UL << 1)   /* 更新文件列表事件 */

//ota消息事件组
#define OTA_MSG_EVT_START       (1UL << 0)   /* 开始 OTA */
#define OTA_MSG_EVT_CANCEL      (1UL << 1)   /* 取消 OTA */

#endif  /* EVENT_CONF_H */