#include "ota_confirm.h"

#include "cmsis_os2.h"
#include "ota_flag.h"
#include "ota_info.h"

#include <stdio.h>

/** Trial window: product logic runs 20s after boot while OTA_INSTALLED. */
#define OTA_TRIAL_CONFIRM_DELAY_MS   20000U

void OTA_Trial_Confirm_Task(void *argument)
{
    OTA_Flag_t fl;
    OTA_Update_Result_t r;

    (void)argument;

    for (;;) {
        r = OTA_GetValidFlag(&fl);
        if (r != OTA_OK) {
            (void)osDelay(500U);
            continue;
        }

        if (fl.ota_state != OTA_INSTALLED) {
            osDelay(1000U);
            continue;
        }

        osDelay(OTA_TRIAL_CONFIRM_DELAY_MS);

        r = OTA_AppConfirmTrialSuccess();
        if (r != OTA_OK) {
            printf("OTA trial confirm failed, ret=%d\r\n", (int)r);
        } else {
            printf("OTA trial confirm OK\r\n");
        }

        for (;;) {
            osDelay(1000U);
        }
    }
}
