#ifndef __SCOPE_CORE_H
#define __SCOPE_CORE_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

#ifndef ADC_SAMPLES_PER_BLOCK
#define ADC_SAMPLES_PER_BLOCK 256
#endif

// 定义图表显示的点数等于采样点数
#define CHART_POINTS ADC_SAMPLES_PER_BLOCK 

typedef enum {
    AUTO_IDLE, AUTO_PREPARE, AUTO_SET_GEAR, AUTO_WAIT_STABLE, 
    AUTO_ANALYZE, AUTO_LOCKED, AUTO_FAIL
} AutoState_t;

typedef enum {
    TRIG_STATE_WAIT, TRIG_STATE_AUTO_RUN, TRIG_STATE_LOCKED
} TrigStatus_t;

typedef enum {
    UI_MODE_SCOPE = 0, // 时域示波器
    UI_MODE_FFT,       // 频域分析
    UI_MODE_PLAYBACK   // 波形回放
} UIMode_t;

typedef struct {
    // --- 核心控制 ---
    volatile uint32_t SampleRate;     
    volatile AutoState_t AutoState;   
    volatile uint8_t  ForceTrigMode;
    // --- 交互状态 ---
    volatile uint8_t  RunState;
    volatile UIMode_t ViewMode;
    
    // --- 菜单状态 ---
    volatile uint8_t  MenuVisible;
    volatile int      MenuIndex; 
} Scope_Config_t;

typedef struct {
    // 1. 测量值
    float Vmax, Vmin, Vrms, Freq;
    int Duty;
    TrigStatus_t TrigStatus;

    // 2. 波形数据：恢复为完整长度
    int16_t ChartData[CHART_POINTS]; 
    
    // 3. 文本信息
    char Text_Info[128];
    
} Scope_ViewData_t;

extern Scope_Config_t   g_Config;         
extern Scope_ViewData_t g_ViewBuf[2];     
extern volatile uint8_t g_front_idx;      

void Task_Scope_Core(void *pvParameters);
void Scope_Playback_Init(void);
void Scope_Playback_Scroll(int16_t steps);
#endif