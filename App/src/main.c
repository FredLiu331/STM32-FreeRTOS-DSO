#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lcd.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "scope_core.h"
#include "adc.h"
#include "key.h"
#include "led.h"
#include "dac.h"
#include <stdio.h>
#include <stdarg.h>
#include "const_data.h"
#include "arm_math.h"
#include "remote.h"
#include "gui_app.h"
#include "rec_app.h"
#include "sys_monitor.h"

#define MENU_ITEM_COUNT 3

// ================= DAC 信号源 (正弦 / 方波 / 三角波) =================
#define DAC_LEN 1024
uint16_t dac_buf[DAC_LEN];

typedef enum {
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_TRIANGLE
} WaveType_t;

// 填充波形数据
void Generate_Wave_Buffer(WaveType_t type)
{
    int32_t wave_val = 0;
    uint8_t table_idx = 0;
    for(int i=0; i<DAC_LEN; i++) {
        switch(type) {
            case WAVE_SINE:
                // 获取 0-255 的索引
                table_idx = (i >> 2); 
                
                // 定点运算公式：
                // 1. g_SineTable[idx] 范围 ±32767
                // 2. 乘以振幅 2047 -> 结果范围 ±67,074,049 (int32_t 可容纳)
                // 3. 右移 15 位 (除以 32768) -> 归一化回振幅
                // 4. 加上偏置 2047
                
                wave_val = ((int32_t)g_SineTable[table_idx] * 2047) >> 15;
                dac_buf[i] = (uint16_t)(2047 + wave_val);
                break;
                
            case WAVE_SQUARE:
                // 前半段高电平(3.3V)，后半段0V
                if(i < DAC_LEN / 2) dac_buf[i] = 4095;
                else dac_buf[i] = 0;
                break;
                
            case WAVE_TRIANGLE:
                // 0 -> 4095 -> 0 线性变化
                if(i < DAC_LEN / 2) {
                    // 上升沿: i=0->0, i=511->4095
                    dac_buf[i] = (uint16_t)(i * 4095UL / (DAC_LEN / 2));
                } else {
                    // 下降沿: i=512->4095, i=1023->0
                    dac_buf[i] = (uint16_t)((DAC_LEN - i) * 4095UL / (DAC_LEN / 2));
                }
                break;
        }
    }
    // 数据变了，必须重新初始化 DMA 源地址
    DAC_Wave_Init(dac_buf, DAC_LEN);
}

void Set_DAC_Signal(int index)
{
    WaveType_t type = WAVE_SINE;
    uint32_t freq = 50;
    
    switch(index) {
        case 0: type = WAVE_SINE;     freq = 50;   break;
        case 1: type = WAVE_SINE;     freq = 1000; break;
        case 2: type = WAVE_SQUARE;   freq = 200;  break;
        case 3: type = WAVE_SQUARE;   freq = 5000; break;
        case 4: type = WAVE_TRIANGLE; freq = 50;   break;
        case 5: type = WAVE_TRIANGLE; freq = 2000; break;
        default: type = WAVE_SINE;    freq = 50;   break;
    }

    Generate_Wave_Buffer(type);
    DAC_Set_Sample_Rate(freq * DAC_LEN); 
    DAC_Playback_Start();
}


// ================= Input & Control 任务  =================
const uint32_t g_speeds[] = {
    100000, 50000, 20000, 10000, 5000, 2000, 1000, 500, 200, 100, 50
};
#define AUTO_SPEED_COUNT (sizeof(g_speeds)/sizeof(uint32_t))

// 查找当前采样率在表中的位置
int Get_Timebase_Index(uint32_t current_sr) {
    for(int i=0; i<AUTO_SPEED_COUNT; i++) {
        if(g_speeds[i] >= current_sr) return i;
    }
    return AUTO_SPEED_COUNT - 1; // 默认最高
}

void Task_Input(void *pvParameters)
{
    // uint8_t key = 0;
    uint8_t rmt = 0;

    // 初始化默认状态
    Set_DAC_Signal(0); 
    g_Config.SampleRate = 50000;
    g_Config.AutoState = AUTO_IDLE;
    g_Config.RunState = 1;
    g_Config.ViewMode = UI_MODE_SCOPE;
    g_Config.MenuVisible = 0;
    g_Config.MenuIndex = 0;
    
    ADC_Set_Freq(g_Config.SampleRate);

    // 局部变量
    int current_tb_idx = Get_Timebase_Index(g_Config.SampleRate);
    static int g_auto_gear_idx = 0; 
    static uint32_t g_auto_wait_start = 0;
    static uint32_t g_auto_wait_ms = 0;
    while(1)
    {
        // key = KEY_Scan();
        rmt = Remote_Scan();
        // if(key == 1) rmt = REMOTE_TEST; // KEY0 -> Auto Set
        // if(key == 2) rmt = REMOTE_MENU; // KEY1 -> Menu

        if(rmt != 0)
        {
            // GUI_Log("Key: 0x%02X", rmt); // 显示键值

            // 层级 1: 全局通用功能
            if(rmt == REMOTE_POWER) {
                g_Config.RunState = !g_Config.RunState;
                if(g_Config.RunState) {
                    g_Config.ForceTrigMode = 0; // 恢复触发
                    GUI_Log("System: RUN");
                } else {
                    g_Config.ForceTrigMode = 1; // 强制等待(暂停更新)
                    GUI_Log("System: STOP");
                }
            }

            // 层级 2: 菜单模式
            else if(g_Config.MenuVisible)
            {
                switch(rmt) {
                    case REMOTE_MENU: 
                    case REMOTE_BACK:
                        g_Config.MenuVisible = 0; // 关闭菜单
                        GUI_Log("Menu: Closed");
                        break;
                        
                    case REMOTE_PLUS: // 向上
                        g_Config.MenuIndex--;
                        if(g_Config.MenuIndex < 0) g_Config.MenuIndex = MENU_ITEM_COUNT - 1;
                        break;
                        
                    case REMOTE_MINUS: // 向下
                        g_Config.MenuIndex++;
                        if(g_Config.MenuIndex >= MENU_ITEM_COUNT) g_Config.MenuIndex = 0;
                        break;
                        
                    case REMOTE_PLAY:  // 确认/进入
                    case REMOTE_RIGHT:
                        // 执行菜单功能
                        if(g_Config.MenuIndex == 0) {
                            g_Config.ViewMode = UI_MODE_SCOPE;
                            GUI_Log("Mode: Scope");
                        }
                        else if(g_Config.MenuIndex == 1) {
                            g_Config.ViewMode = UI_MODE_FFT;
                            GUI_Log("Mode: FFT");
                        }
                        else if(g_Config.MenuIndex == 2) {
                            Scope_Playback_Init();
                            g_Config.ViewMode = UI_MODE_PLAYBACK;
                            GUI_Log("Mode: Playback");
                        }
                        g_Config.MenuVisible = 0; // 选完自动关闭
                        break;
                }
            }
            
            // 层级 3: 监控模式
            else 
            {
                switch(rmt) {
                    // --- 呼出菜单 ---
                    case REMOTE_MENU:
                        g_Config.MenuVisible = 1;
                        g_Config.MenuIndex = 0; // 重置光标
                        GUI_Log("Menu: Open");
                        break;
                        
                    // --- 自动量程 (Auto Set) ---
                    case REMOTE_TEST: 
                        if(g_Config.RunState) 
                        { 
                            g_Config.AutoState = AUTO_PREPARE; 
                            GUI_Log("Auto Set..."); 
                        } 
                        break;
                        
                    // --- 时基调整 (左右键) ---
                    case REMOTE_LEFT: // 放大波形 (降低采样率)
                        if(g_Config.ViewMode == UI_MODE_SCOPE) {
                            current_tb_idx--;
                            if(current_tb_idx < 0) current_tb_idx = 0;
                            uint32_t new_sr = g_speeds[current_tb_idx];
                            g_Config.SampleRate = new_sr;
                            ADC_Set_Freq(new_sr);
                            GUI_Log("SR: %d Hz", new_sr);
                        }
                        else if(g_Config.ViewMode == UI_MODE_PLAYBACK) {
                            Scope_Playback_Scroll(-128); // 每次移动半屏
                            GUI_Log("Seek <--");
                        }
                        break;
                        
                    case REMOTE_RIGHT:
                        if(g_Config.ViewMode == UI_MODE_SCOPE) {
                            current_tb_idx++;
                            if(current_tb_idx >= AUTO_SPEED_COUNT) current_tb_idx = AUTO_SPEED_COUNT - 1;
                            uint32_t new_sr = g_speeds[current_tb_idx];
                            g_Config.SampleRate = new_sr;
                            ADC_Set_Freq(new_sr);
                            GUI_Log("SR: %d Hz", new_sr);
                        }
                        else if(g_Config.ViewMode == UI_MODE_PLAYBACK) {
                            Scope_Playback_Scroll(128);
                            GUI_Log("Seek -->");
                        }
                        break;

                    case REMOTE_C: 
                        if(Rec_Get_State_String()[0] == 'R') { // is REC?
                            Rec_Controller_Stop();
                            GUI_Log("CMD: Stop Rec");
                        } else {
                            Rec_Controller_Start();
                            GUI_Log("CMD: Start Rec");
                        }
                        break;

                    // --- 信号发生器快捷键 ---
                    case REMOTE_0: Set_DAC_Signal(0); GUI_Log("Out: Sine 50Hz"); break;
                    case REMOTE_1: Set_DAC_Signal(1); GUI_Log("Out: Sine 1kHz"); break;
                    case REMOTE_2: Set_DAC_Signal(2); GUI_Log("Out: Square 200Hz"); break;
                    case REMOTE_3: Set_DAC_Signal(3); GUI_Log("Out: Square 5kHz"); break;
                    case REMOTE_4: Set_DAC_Signal(4); GUI_Log("Out: Tri 50Hz"); break;
                    case REMOTE_5: Set_DAC_Signal(5); GUI_Log("Out: Tri 2kHz"); break;
                }
            }
        }

        // 2. === Auto 状态机调度 ===
        switch(g_Config.AutoState)
        {
            case AUTO_IDLE:
            case AUTO_LOCKED:
            case AUTO_FAIL:
                break;

            case AUTO_PREPARE:
                GUI_Log("Auto: Start Scanning...");
                g_auto_gear_idx = 0; // 从最高档开始
                g_Config.AutoState = AUTO_SET_GEAR;
                break;

            case AUTO_SET_GEAR:
            {
                uint32_t current_sr = g_speeds[g_auto_gear_idx];
                
                ADC_Set_Freq(current_sr);
                g_Config.SampleRate = current_sr; 
                
                // 等待时间计算
                g_auto_wait_ms = (ADC_SAMPLES_PER_BLOCK * 1200) / current_sr;
                
                if(g_auto_wait_ms < 150) g_auto_wait_ms = 150;
                if(g_auto_wait_ms > 6000) g_auto_wait_ms = 6000;
                
                g_auto_wait_start = xTaskGetTickCount();
                g_Config.AutoState = AUTO_WAIT_STABLE;
                break;
            }

            case AUTO_WAIT_STABLE:
                if((xTaskGetTickCount() - g_auto_wait_start) * portTICK_PERIOD_MS >= g_auto_wait_ms) {
                    g_Config.AutoState = AUTO_ANALYZE;
                }
                break;

            case AUTO_ANALYZE:
            {
                uint8_t idx = g_front_idx; 
                Scope_ViewData_t *pView = &g_ViewBuf[idx];

                float current_vpp = pView->Vmax - pView->Vmin;

                // 噪声/无信号快速跳过
                if(current_vpp < 0.15f) {
                    // 信号太弱，视为噪声，直接下一档
                }
                else if(pView->Freq > 1.0f) 
                {
                    float freq_in = pView->Freq;
                    uint32_t current_sr = g_Config.SampleRate;
                    
                    GUI_Log("Est: %.1fHz (at %dHz)", freq_in, current_sr);

                    // 计算目标采样率 (屏幕显示约 5 个周期)
                    uint32_t target_sr = (uint32_t)(freq_in * ADC_SAMPLES_PER_BLOCK / 5);
                    
                    // 查找最匹配档位
                    int best_idx = -1;
                    uint32_t min_diff = 0xFFFFFFFF;
                    for(int k=0; k < AUTO_SPEED_COUNT; k++) {
                        uint32_t diff = (g_speeds[k] > target_sr) ? (g_speeds[k] - target_sr) : (target_sr - g_speeds[k]);
                        if(g_speeds[k] < target_sr) diff *= 2; 
                        if(diff < min_diff) {
                            min_diff = diff;
                            best_idx = k;
                        }
                    }
                    
                    if(best_idx != -1) {
                        ADC_Set_Freq(g_speeds[best_idx]);
                        g_Config.SampleRate = g_speeds[best_idx];
                        GUI_Log("Lock: %dHz", g_speeds[best_idx]);
                        g_Config.AutoState = AUTO_LOCKED;
                        break; 
                    }
                }
                
                // 下一档
                g_auto_gear_idx++;
                if(g_auto_gear_idx >= AUTO_SPEED_COUNT) {
                    // 低频/失败保护
                    ADC_Set_Freq(50);
                    g_Config.SampleRate = 50;
                    GUI_Log("Auto Fail");
                    g_Config.AutoState = AUTO_FAIL;
                } else {
                    g_Config.AutoState = AUTO_SET_GEAR; 
                }
                break;
            }
        }

        vTaskDelay(50);
    }
}


// ================= Main =================
void Task_Init(void *pvParameters)
{
    KEY_Init();
    LED_Init();
    LCD_Init();
    Remote_Init();
    
    lv_init();
    lv_port_disp_init();
    
    xTaskCreate(Task_GUI, "GUI", 768, NULL, 2, NULL);
    xTaskCreate(Task_Scope_Core, "ScopeCore", 512, NULL, 5, NULL);
    xTaskCreate(Task_Input, "Input", 256, NULL, 3, NULL);
    xTaskCreate(Task_Recorder, "Recorder", 256, NULL, 4, NULL);
    xTaskCreate(Task_SysMonitor, "Monitor", 128, NULL, 1, NULL);

    vTaskDelete(NULL);
}

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    
    xTaskCreate(Task_Init, "Init", 512, NULL, 6, NULL);
    
    vTaskStartScheduler();
    
    while(1);
}