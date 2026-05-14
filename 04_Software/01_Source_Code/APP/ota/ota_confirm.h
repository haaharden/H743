#ifndef OTA_CONFIRM_H
#define OTA_CONFIRM_H

#ifdef __cplusplus
extern "C" {
#endif

/** FreeRTOS task: after OTA_INSTALLED, delay 20s then promote staging slot to committed. */
void OTA_Trial_Confirm_Task(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* OTA_CONFIRM_H */
