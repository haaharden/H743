/**
 * @file lv_port_indev_templ.c
 *
 */

/*Copy this file as "lv_port_indev.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "touch_gt1151q.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_indev_t * indev_touchpad;
static uint8_t s_touch_ready = 0U;
static lv_indev_state_t touchpad_state = LV_INDEV_STATE_REL;
static lv_point_t s_touch_last_point = {0, 0};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    touchpad_init();

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*------------------
 * Touchpad
 * -----------------*/

/*Initialize your touchpad*/
static void touchpad_init(void)
{
    // 触摸初始化失败时，LVGL 输入保持释放态，避免把上层流程一起带崩。
    if (Touch_Init() == HAL_OK)
    {
        s_touch_ready = 1U;
    }
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    uint16_t touch_x = 0U;
    uint16_t touch_y = 0U;

    LV_UNUSED(indev_drv);

    if (s_touch_ready == 0U)
    {
        data->state = LV_INDEV_STATE_REL;
        data->point.x = 0;
        data->point.y = 0;
        data->continue_reading = false;
        return;
    }

    // 这里直接轮询坐标，先确保功能通，再把中断优化加回去。
    // 如果只依赖中断标志，一旦 INT 边沿方向和板子实际不一致，就会表现成“完全没反应”。
    if (Touch_ReadPoint(&touch_x, &touch_y) != 0U)
    {
        s_touch_last_point.x = (lv_coord_t)touch_x;
        s_touch_last_point.y = (lv_coord_t)touch_y;
        touchpad_state = LV_INDEV_STATE_PR;
    }
    else
    {
        touchpad_state = LV_INDEV_STATE_REL;
    }

    // 中断标志现在只作为辅助观测量保留，不再参与读点门控。
    Touch_ClearEventPending();

    data->state = touchpad_state;
    data->point = s_touch_last_point;
    data->continue_reading = false;
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
