/*
给usb_host提供接口，usb插拔通知
*/
#ifndef APP_USBHOST_PORT_H
#define APP_USBHOST_PORT_H

#include "cmsis_os2.h"//h文件中include表示这个依赖是对外接口的一部分，c文件中include表示这个依赖是实现细节的一部分

void app_usb_host_port_init(osEventFlagsId_t event_handle);
void app_usb_disconnected(void);
void app_usb_ready(void);

#endif /* APP_USBHOST_PORT_H */
