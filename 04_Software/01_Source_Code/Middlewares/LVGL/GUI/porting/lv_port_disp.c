/**
 * @file lv_port_disp_templ.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include "disp_ht05.h"
#include <stdbool.h>

#if LV_COLOR_DEPTH != 16
#error "当前显示链路按 RGB565 接入，LV_COLOR_DEPTH 必须为 16"
#endif

/*********************
 *      DEFINES
 *********************/
#ifndef LV_PORT_DISP_HOR_RES
    #define LV_PORT_DISP_HOR_RES    LCD_PIXEL_WIDTH
#endif

#ifndef LV_PORT_DISP_VER_RES
    #define LV_PORT_DISP_VER_RES    LCD_PIXEL_HEIGHT
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

/**********************
 *  STATIC VARIABLES
 **********************/
static volatile bool disp_flush_enabled = true;  // 先保留刷新总开关，后面真接屏时可以继续沿用。
static lv_disp_draw_buf_t s_draw_buf;            // 直接把两块整屏 SDRAM 当作 LVGL 双缓冲。
static lv_color_t * s_buf_1;
static lv_color_t * s_buf_2;
/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    // 这里完成 LVGL 显示驱动注册，并把 flush 直接接到当前工程的 LTDC/DMA2D 显示链路。
    disp_init();

    lv_disp_draw_buf_init(&s_draw_buf, s_buf_1, s_buf_2, LV_PORT_DISP_HOR_RES * LV_PORT_DISP_VER_RES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LV_PORT_DISP_HOR_RES;
    disp_drv.ver_res = LV_PORT_DISP_VER_RES;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &s_draw_buf;
    disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    // 这里直接复用当前工程已经初始化好的 LTDC 显示链路。
    lcd_init();
    s_buf_1 = (lv_color_t *)LCD_FB1_ADDRESS;
    s_buf_2 = (lv_color_t *)LCD_FB0_ADDRESS;
}

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    LV_UNUSED(area);

    if(disp_flush_enabled == false) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    // 全屏双缓冲模式下，LVGL 会把整帧画到 color_p 指向的那块 SDRAM，flush 时只需要切 LTDC 地址。
    LCD_SetActiveBufferAddress((uint32_t)color_p);
    lv_disp_flush_ready(disp_drv);
}


#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
