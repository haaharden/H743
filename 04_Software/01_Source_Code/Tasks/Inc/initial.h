#ifndef initial_h
#define initial_h

typedef enum {
  USB_MODE_IDLE = 0,
  USB_MODE_HOST
} usb_mode_t;

extern usb_mode_t usb_mode;

void Initial_Task(void *argument);

#endif 