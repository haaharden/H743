#include "gui_test_app.h"

#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_port_flash_fs.h"

lv_ui guider_ui;  // 生成代码里的事件跳转直接引用 guider_ui，这里补成工程内唯一实例。

void GUI_TestApp_Init(void)
{
    // 这里只做最小可用初始化，不主动改你当前的 main/task 调用链。
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    lv_port_flash_fs_init();
    setup_ui(&guider_ui);
}

void GUI_TestApp_TaskHandler(void)
{
    // 后续你只需要在循环或任务里周期调用这里即可推动 LVGL 刷新。
    lv_timer_handler();
}