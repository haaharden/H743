#include "cmsis_os2.h"
#include "display.h"
#include "gui_test_app.h"
#include "custom.h"
#include "events_init.h"
#include "event_conf.h"
#include "usb_file_browser.h"
#include "string.h"

static lv_obj_t *s_last_screen = NULL;
static uint8_t s_in_update_screen = 0;

extern osSemaphoreId_t LvglReadySemHandle;
extern osEventFlagsId_t uiMSGEventHandle;
extern osEventFlagsId_t otaEventHandle;

// 进入更新界面，保存当前界面以便后续返回。
static void ui_enter_update_screen(void)
{
    lv_obj_t *act = lv_scr_act();

    /*
     * 如果当前已经在 screen2，就不要重复保存。
     * 否则 s_last_screen 可能会被保存成 screen2 自己。
     */
    if (guider_ui.screen_2 == NULL) {
        setup_scr_screen_2(&guider_ui);
    }

    if (act != guider_ui.screen_2)
    {
        s_last_screen = act;
    }

    s_in_update_screen = 1;

    /*
     * 注意最后一个参数不要用 true。
     * GUI Guider 生成的 screen 通常是全局对象，不应该自动删除。
     */
    lv_scr_load_anim(guider_ui.screen_2,
                     LV_SCR_LOAD_ANIM_NONE,
                     0,
                     0,
                     false);
}

// 从更新界面返回到之前的界面，如果没有保存之前的界面则返回主界面。
static void ui_back_to_last_screen(void)
{
    lv_obj_t *target = NULL;

    if (s_in_update_screen && s_last_screen != NULL)
    {
        target = s_last_screen;
    }
    else
    {
        target = guider_ui.screen_home;   // 没有保存界面时，回默认主界面
    }

    lv_scr_load_anim(target,
                     LV_SCR_LOAD_ANIM_NONE,
                     0,
                     0,
                     false);

    s_in_update_screen = 0;
    s_last_screen = NULL;
    //guider_ui.screen_2 = NULL;  // 生成的 screen2 是全局对象，手动置空表示当前没有在用它。
}

//判断文件是不是 app.bin
static uint8_t ui_is_app_bin(const UsbViewItem_t *item)
{
    if (item == NULL || item->is_dir)
    {
        return 0;
    }

    return (strcmp(item->name, "app.bin") == 0 ||
            strcmp(item->name, "APP.BIN") == 0);
}

// USB 文件点击事件回调，判断是否是 app.bin，如果是就弹窗询问是否升级。
static void ui_ota_msgbox_event_cb(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    const char *btn_txt = lv_msgbox_get_active_btn_text(mbox);

    if (btn_txt == NULL)
    {
        return;
    }

    if (strcmp(btn_txt, "Yes") == 0)
    {
        osEventFlagsSet(otaEventHandle, OTA_MSG_EVT_START);
    }

    lv_msgbox_close(mbox);
}

//文件点击事件回调，判断是否是 app.bin，如果是就弹窗询问是否升级。
static void ui_usb_item_event_cb(lv_event_t *e)
{
    const UsbViewItem_t *item = lv_event_get_user_data(e);

    if (!ui_is_app_bin(item))
    {
        return;
    }

    static const char *btns[] = {"Yes", "No", ""};

    lv_obj_t *mbox = lv_msgbox_create(NULL,
                                      "OTA Update",
                                      "Found app.bin. Upgrade now?",
                                      btns,
                                      false);

    lv_obj_add_event_cb(mbox,
                        ui_ota_msgbox_event_cb,
                        LV_EVENT_VALUE_CHANGED,
                        NULL);

    lv_obj_center(mbox);
}

//实现单个文件/文件夹图标的创建，index 是 0~17 的显示位置索引，item 是要显示的文件/文件夹信息。
static void ui_create_usb_item(uint8_t index, const UsbViewItem_t *item)
{
    lv_obj_t *tile;
    lv_obj_t *icon;
    lv_obj_t *name_label;

    uint8_t col;
    uint8_t row;

    int x;
    int y;

    col = index % 6;
    row = index / 6;

    x = 10 + col * 130;
    y = 10 + row * 115;

    tile = lv_obj_create(guider_ui.screen_2_cont_files);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tile,
                    ui_usb_item_event_cb,
                    LV_EVENT_CLICKED,
                    (void *)item);

    lv_obj_set_size(tile, 120, 105);
    lv_obj_set_pos(tile, x, y);

    /*
     * 可选：让 tile 看起来更干净
     */
    lv_obj_set_style_pad_all(tile, 0, 0);

    icon = lv_label_create(tile);

    if (item->is_dir)
    {
        lv_label_set_text(icon, LV_SYMBOL_DIRECTORY);
    }
    else
    {
        lv_label_set_text(icon, LV_SYMBOL_LIST);
    }

    /*
     * 图标字体变大
     * 注意：lv_font_montserrat_32 需要在 lv_conf.h 里开启
     */
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_32, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 10);

    name_label = lv_label_create(tile);
    lv_label_set_text(name_label, item->name);

    /*
     * 关键：固定 label 宽度
     */
    lv_obj_set_width(name_label, 110);

    /*
     * 关键：让文字在 label 内部居中
     */
    lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_CENTER, 0);

    /*
     * 文件名字体变大
     * 可以先用 16 或 18
     */
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_16, 0);

    /*
     * 文件名过长时省略
     */
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT);

    lv_obj_align(name_label, LV_ALIGN_BOTTOM_MID, 0, -8);
}

// 显示 USB 根目录下的文件和文件夹
static void ui_show_usb_root_items(void)
{
    uint8_t i;

    lv_label_set_text(guider_ui.screen_2_label_path,
                      g_usb_view_model.path);

    lv_obj_clean(guider_ui.screen_2_cont_files);

    for (i = 0; i < g_usb_view_model.count; i++)
    {
        ui_create_usb_item(i, &g_usb_view_model.items[i]);
    }
}


void Display_Task(void *argument)
{
  osSemaphoreAcquire(LvglReadySemHandle, osWaitForever);
  osSemaphoreDelete(LvglReadySemHandle);
  // 只在 LVGL 初始化完成后创建界面对象，避免与 initial() 并发访问 LVGL 内部内存池。
  setup_ui(&guider_ui);
  events_init(&guider_ui);
  custom_init(&guider_ui);
  for(;;)
  {
    uint32_t flags = osEventFlagsWait(uiMSGEventHandle, 
                            UI_MSG_EVT_DISC | UI_MSG_EVT_UPDATE, 
                            osFlagsWaitAny, 
                            0);
    if ((flags & osFlagsError) == 0U) {
      if ((flags & UI_MSG_EVT_DISC) != 0U){
          ui_back_to_last_screen();
      }
      else if ((flags & UI_MSG_EVT_UPDATE) != 0U)
      {
          ui_enter_update_screen();
          ui_show_usb_root_items();
      }
    }
    // 这里放显示相关的周期性任务，比如 LVGL 的 lv_timer_handler 调度。
    lv_timer_handler();
    osDelay(10);  // LVGL 官方推荐 5~10ms 的调度周期。
  }
}
