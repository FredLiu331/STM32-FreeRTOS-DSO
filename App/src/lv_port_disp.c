#include "lvgl.h"
#include "lcd.h"

/* ST7789: 240 * 320 */
#define DISP_BUF_SIZE (240 * 10) 

static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf_1[DISP_BUF_SIZE]; // 缓冲区

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    LCD_Color_Fill(area->x1, area->y1, area->x2, area->y2, (uint16_t *)color_p);

    lv_disp_flush_ready(disp_drv);
}

void lv_port_disp_init(void)
{
    lv_disp_draw_buf_init(&draw_buf, buf_1, NULL, DISP_BUF_SIZE);

    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = 240;
    disp_drv.ver_res = 320;

    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    
    lv_disp_drv_register(&disp_drv);
}