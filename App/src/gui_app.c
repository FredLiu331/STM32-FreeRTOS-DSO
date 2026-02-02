#include "gui_app.h"
#include "scope_core.h" 
#include "lvgl.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <stdarg.h>
#include "sys_monitor.h"

// ================= 日志系统 =================
#define LOG_MSG_LEN 64
#define LOG_QUEUE_LEN 10
QueueHandle_t xLogQueue;

void GUI_Log(const char *fmt, ...)
{
    char buf[LOG_MSG_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, LOG_MSG_LEN, fmt, args);
    va_end(args);
    if(xLogQueue != NULL) xQueueSend(xLogQueue, buf, 0);
}

static lv_obj_t * chart_obj;
static lv_chart_series_t * ser1;
static lv_obj_t * label_info;
static lv_obj_t * ta_log;
static lv_obj_t * label_sys_stats;

// 菜单相关组件
static lv_obj_t * ui_menu_cont;
static lv_obj_t * ui_menu_btns[3];
#define MENU_ITEM_COUNT 3


/**
 * @brief  创建弹出式菜单 (默认隐藏)
 */
static void GUI_Create_Menu(void)
{

    ui_menu_cont = lv_list_create(lv_scr_act());
    lv_obj_set_size(ui_menu_cont, 140, 110);
    lv_obj_center(ui_menu_cont); // 屏幕居中
    
    // 样式：深灰背景，白色边框，像个弹窗
    lv_obj_set_style_bg_color(ui_menu_cont, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_border_color(ui_menu_cont, lv_color_white(), 0);
    lv_obj_set_style_border_width(ui_menu_cont, 2, 0);
    
    // Index 0: Scope
    ui_menu_btns[0] = lv_list_add_btn(ui_menu_cont, LV_SYMBOL_CHARGE, "Scope Mode");
    // Index 1: FFT
    ui_menu_btns[1] = lv_list_add_btn(ui_menu_cont, LV_SYMBOL_AUDIO, "FFT Analysis");
    // Index 2: Playback
    ui_menu_btns[2] = lv_list_add_btn(ui_menu_cont, LV_SYMBOL_SD_CARD, "Playback");

    // 初始状态：隐藏
    lv_obj_add_flag(ui_menu_cont, LV_OBJ_FLAG_HIDDEN);
}

static void GUI_Refresh_Menu(void)
{
    // 1. 处理可见性
    if (g_Config.MenuVisible) {
        lv_obj_clear_flag(ui_menu_cont, LV_OBJ_FLAG_HIDDEN); // 显示
        
        // 确保显示在最上层
        lv_obj_move_foreground(ui_menu_cont); 
    } else {
        lv_obj_add_flag(ui_menu_cont, LV_OBJ_FLAG_HIDDEN);   // 隐藏
        return;
    }

    // 2. 处理光标高亮 (根据 MenuIndex)
    for(int i=0; i<MENU_ITEM_COUNT; i++) {
        if(i == g_Config.MenuIndex) {
            // 选中：背景变蓝，文字变白
            lv_obj_set_style_bg_color(ui_menu_btns[i], lv_palette_main(LV_PALETTE_BLUE), 0);
            lv_obj_set_style_text_color(ui_menu_btns[i], lv_color_white(), 0);
        } else {
            // 未选中：背景透明/深色，文字灰色
            lv_obj_set_style_bg_color(ui_menu_btns[i], lv_palette_darken(LV_PALETTE_GREY, 3), 0);
            lv_obj_set_style_text_color(ui_menu_btns[i], lv_color_white(), 0);
        }
    }
}

static void GUI_Setup_Layout(void)
{
    // 全局背景
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);

    // Chart
    chart_obj = lv_chart_create(lv_scr_act());
    lv_obj_set_size(chart_obj, 240, 120);       
    lv_obj_align(chart_obj, LV_ALIGN_TOP_MID, 0, 45); 

    lv_chart_set_type(chart_obj, LV_CHART_TYPE_LINE); 
    lv_chart_set_point_count(chart_obj, CHART_POINTS); 
    lv_chart_set_range(chart_obj, LV_CHART_AXIS_PRIMARY_Y, 0, 4100);
    lv_chart_set_div_line_count(chart_obj, 5, 4); 

    lv_obj_set_style_bg_color(chart_obj, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_color(chart_obj, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_MAIN);
    lv_obj_set_style_size(chart_obj, 0, LV_PART_INDICATOR); 
    lv_obj_set_style_line_width(chart_obj, 2, LV_PART_ITEMS); 
    
    ser1 = lv_chart_add_series(chart_obj, lv_palette_main(LV_PALETTE_LIME), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_ext_y_array(chart_obj, ser1, (lv_coord_t *)g_ViewBuf[0].ChartData);

    // Label
    label_info = lv_label_create(lv_scr_act());
    lv_obj_align(label_info, LV_ALIGN_TOP_LEFT, 5, 5); 
    lv_obj_set_width(label_info, 230); 
    lv_obj_set_style_text_color(label_info, lv_color_white(), 0);

    // Log
    ta_log = lv_textarea_create(lv_scr_act());
    lv_obj_set_size(ta_log, 230, 70); 
    lv_obj_align(ta_log, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(ta_log, LV_OBJ_FLAG_CLICKABLE); 
    
    lv_obj_set_style_bg_color(ta_log, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ta_log, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_text_color(ta_log, lv_palette_lighten(LV_PALETTE_GREY, 4), LV_PART_MAIN);
    lv_obj_set_style_border_width(ta_log, 0, LV_PART_MAIN);
    
    // SYS
    label_sys_stats = lv_label_create(lv_scr_act());
    lv_obj_set_width(label_sys_stats, 240);
    lv_obj_align_to(label_sys_stats, chart_obj, LV_ALIGN_OUT_BOTTOM_LEFT, 5, 2);
    lv_obj_set_style_text_color(label_sys_stats, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_set_style_text_line_space(label_sys_stats, 0, 0); 
    lv_label_set_text(label_sys_stats, "CPU: --.-%");

    GUI_Create_Menu();
}

static void GUI_Update_Log(void)
{
    char log_recv_buf[LOG_MSG_LEN + 4];
    while(xQueueReceive(xLogQueue, log_recv_buf, 0) == pdTRUE) {
        lv_textarea_add_text(ta_log, log_recv_buf);
        lv_textarea_add_text(ta_log, "\n"); 
    }
}


static void GUI_Refresh_Chart(Scope_ViewData_t *pView)
{
    static UIMode_t last_mode = UI_MODE_SCOPE;

    // 模式切换检测
    if (g_Config.ViewMode != last_mode) 
    {
        if (g_Config.ViewMode == UI_MODE_FFT) {
            lv_chart_set_type(chart_obj, LV_CHART_TYPE_BAR);
            lv_chart_set_point_count(chart_obj, 64); // FFT显示抽样
        } else {
            lv_chart_set_type(chart_obj, LV_CHART_TYPE_LINE);
            lv_chart_set_point_count(chart_obj, CHART_POINTS);
        }
        last_mode = g_Config.ViewMode;
    }

    // 更新数据源
    lv_chart_set_ext_y_array(chart_obj, ser1, (lv_coord_t *)pView->ChartData);
    lv_chart_refresh(chart_obj); 
}

void Task_GUI(void *pvParameters)
{
    // 初始化资源
    xLogQueue = xQueueCreate(LOG_QUEUE_LEN, LOG_MSG_LEN);
    
    uint32_t stats_timer = 0;
    char sys_txt_buf[512];

    // 建立界面
    GUI_Setup_Layout();
    GUI_Log("Scope UI Ready.");

    while(1)
    {
        // 处理 Log
        GUI_Update_Log();

        // 获取数据
        uint8_t current_idx = g_front_idx;
        Scope_ViewData_t *pView = &g_ViewBuf[current_idx];

        // 刷新业务组件
        GUI_Refresh_Chart(pView);
        
        // 刷新菜单状态
        GUI_Refresh_Menu();

        lv_label_set_text(label_info, pView->Text_Info);

        stats_timer += 20;
        if(stats_timer >= 200) {
            stats_timer = 0;
            Sys_Get_Info_Text(sys_txt_buf, sizeof(sys_txt_buf));
            // GUI_Log("Stats len: %d", strlen(sys_txt_buf));
            lv_label_set_text(label_sys_stats, sys_txt_buf);
        }

        // 驱动 LVGL
        lv_timer_handler();
        
        // 动态帧率
        if(pView->TrigStatus == TRIG_STATE_WAIT) {
            vTaskDelay(100);
        } 
        else if (g_Config.ViewMode == UI_MODE_FFT) {
            vTaskDelay(66); 
        }
        else {
            vTaskDelay(20);
        }
    }
}