/*
实现usb事件通知功能，usb事件包括：usb断开、usb准备就绪等
*/
#include "app_usbhost_port.h"
#include "event_conf.h"

static osEventFlagsId_t s_usb_event_handle = NULL;

//依赖注入，防止extern，可直接复用
void app_usb_host_port_init(osEventFlagsId_t event_handle)
{
  s_usb_event_handle = event_handle;
}

void app_usb_disconnected(void)
{
  if (s_usb_event_handle != NULL) {
    osEventFlagsSet(s_usb_event_handle, USB_EVT_DISCONNECT);
  }
}

void app_usb_ready(void)
{
  if (s_usb_event_handle != NULL) {
    osEventFlagsSet(s_usb_event_handle, USB_EVT_READY);
  }
}