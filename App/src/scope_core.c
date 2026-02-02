#include "scope_core.h"
#include "adc.h"
#include "arm_math.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "rec_app.h"
#include "w25qxx.h"

Scope_Config_t   g_Config = { .SampleRate = 50000, .AutoState = AUTO_IDLE, .ForceTrigMode = 0 };
Scope_ViewData_t g_ViewBuf[2];
volatile uint8_t g_front_idx = 0; 
// 滤波相关
#define FREQ_AVG_COUNT 5
static float    s_freq_filtered = 0.0f;
// FFT相关
#define FFT_LEN 256
static q15_t fft_input_buf[FFT_LEN * 2];
static q15_t fft_output_mag[FFT_LEN];
extern const arm_cfft_instance_q15 arm_cfft_sR_q15_len256;
// 录制相关
static uint16_t s_RecStagingBuf[128]; 
static uint8_t  s_RecStagingIdx = 0;
static int32_t s_PlaybackOffset = 0;
static int32_t s_TotalRecSize = 0;
static uint8_t s_PlaybackBuf[CHART_POINTS*2]; // 512字节
static uint8_t s_NeedReload = 1;

// --- 回放控制接口 ---
void Scope_Playback_Init(void) {
    s_TotalRecSize = Rec_Get_Recorded_Len() * 1024;
    s_PlaybackOffset = 0;
    s_NeedReload = 1;
}

void Scope_Playback_Scroll(int16_t steps) {
    if(g_Config.ViewMode != UI_MODE_PLAYBACK) return;
    
    int32_t new_offset = s_PlaybackOffset + (steps * 2); // 假设每次步进2字节(1个点)
    int32_t window_bytes = CHART_POINTS * 2;
    int32_t max_offset = s_TotalRecSize - window_bytes;

    // 1. 如果录制内容比屏幕还短，强制锁定在0
    if (max_offset < 0) max_offset = 0;

    // 2. 限制上限
    if(new_offset > max_offset) new_offset = max_offset;
    
    // 3. 限制下限
    if(new_offset < 0) new_offset = 0;
    
    if(new_offset != s_PlaybackOffset) {
        s_PlaybackOffset = new_offset;
        s_NeedReload = 1;
    }
}

static void Process_Recording_Stream(uint16_t *pSource, uint16_t len)
{
    for(int i=0; i<len; i++) {
        // 填入凑包缓冲
        s_RecStagingBuf[s_RecStagingIdx++] = pSource[i];
        
        // 凑满一页 (256字节)
        if(s_RecStagingIdx >= 128) {
            // 推送到 Recorder 任务
            Rec_Push_Data(s_RecStagingBuf, 128);
            s_RecStagingIdx = 0; // 复位索引
        }
    }
}

static q15_t SubTask_Calc_Stats(uint16_t *pRaw, Scope_ViewData_t *pView)
{
    q15_t dsp_mean, dsp_max, dsp_min, dsp_std;
    uint32_t dsp_idx;
    const float k_vol = 3.3f / 4096.0f;

    arm_max_q15((q15_t*)pRaw, ADC_SAMPLES_PER_BLOCK, &dsp_max, &dsp_idx);
    arm_min_q15((q15_t*)pRaw, ADC_SAMPLES_PER_BLOCK, &dsp_min, &dsp_idx);
    arm_mean_q15((q15_t*)pRaw, ADC_SAMPLES_PER_BLOCK, &dsp_mean);
    arm_std_q15((q15_t*)pRaw, ADC_SAMPLES_PER_BLOCK, &dsp_std);


    pView->Vmax = (float)dsp_max * k_vol;
    pView->Vmin = (float)dsp_min * k_vol;
    pView->Vrms = (float)dsp_std * k_vol;

    int high_cnt = 0;
    for(int i=0; i<ADC_SAMPLES_PER_BLOCK; i++) {
        if(pRaw[i] > dsp_mean) high_cnt++;
    }
    pView->Duty = (int)((float)high_cnt * 100.0f / ADC_SAMPLES_PER_BLOCK);

    return dsp_mean;
}

static void SubTask_Calc_Freq(uint16_t *pRaw, Scope_ViewData_t *pView, q15_t mean_val)
{
    // 1. 动态迟滞区间 (Vpp / 10)
    float vpp_vol = pView->Vmax - pView->Vmin;
    uint16_t raw_vpp = (uint16_t)(vpp_vol * 4096.0f / 3.3f);
    
    uint16_t raw_hysteresis = raw_vpp / 10; 
    if(raw_hysteresis < 20) raw_hysteresis = 20; 

    uint16_t th_low  = mean_val - raw_hysteresis;
    uint16_t th_high = mean_val + raw_hysteresis;
    
    uint16_t cross_count = 0;
    int first_edge_idx = -1;
    int last_edge_idx = -1;
    uint8_t current_state = (pRaw[0] >= th_high) ? 1 : 0;
    
    // 2. 遍历过零点
    for(int i=1; i<ADC_SAMPLES_PER_BLOCK; i++) {
        uint16_t val = pRaw[i];
        if(current_state == 0) {
            if(val >= th_high) { 
                current_state = 1; 
                if(first_edge_idx == -1) first_edge_idx = i;
                last_edge_idx = i;
                cross_count++; 
            }
        } else {
            if(val <= th_low) { current_state = 0; }
        }
    }
    
    // 3. 计算瞬时频率
    float current_freq = 0.0f;
    if(cross_count >= 2 && last_edge_idx > first_edge_idx) {
        float periods = (float)(cross_count - 1);
        float samples_span = (float)(last_edge_idx - first_edge_idx);
        current_freq = periods * (float)g_Config.SampleRate / samples_span;
    } else {
        if(cross_count == 0) current_freq = 0.0f;
        else current_freq = pView->Freq; // 保持上一帧
    }

    // 4. IIR 滤波
    if(current_freq < 0.1f) s_freq_filtered = 0.0f;
    else if(fabs(current_freq - s_freq_filtered) > (s_freq_filtered * 0.1f)) s_freq_filtered = current_freq; 
    else s_freq_filtered = s_freq_filtered * 0.5f + current_freq * 0.5f;
    
    pView->Freq = s_freq_filtered;
}

static void SubTask_Process_Wave(uint16_t *pRaw, Scope_ViewData_t *pView, q15_t mean_val)
{
    // === 回放模式 ===
    if (g_Config.ViewMode == UI_MODE_PLAYBACK)
    {
        if(s_NeedReload) {
            W25QXX_Read(s_PlaybackBuf, REC_FLASH_START + s_PlaybackOffset, CHART_POINTS * 2);
            s_NeedReload = 0;
        }
        uint16_t *pData = (uint16_t*)s_PlaybackBuf;
        for(int i=0; i<CHART_POINTS; i++) pView->ChartData[i] = pData[i];
        // for(int i=0; i<CHART_POINTS; i++) pView->ChartData[i] = s_PlaybackBuf[i];
        pView->TrigStatus = TRIG_STATE_WAIT;
    }

    else if (g_Config.ViewMode == UI_MODE_FFT) 
    {
        // === FFT 模式 ===
        for(int i=0; i<FFT_LEN; i++) {
            fft_input_buf[i*2]     = ((int16_t)pRaw[i] - 2048) << 4; 
            fft_input_buf[i*2 + 1] = 0;                              
        }
        
        arm_cfft_q15(&arm_cfft_sR_q15_len256, fft_input_buf, 0, 1);
        arm_cmplx_mag_q15(fft_input_buf, fft_output_mag, FFT_LEN);
        
        fft_output_mag[0] = 0; // 去直流
        
        for(int i=0; i<CHART_POINTS; i++) {
            if(i < FFT_LEN/2) {
                int32_t val = fft_output_mag[i]; 
                // 比如乘以 4 或者 8，具体看信号强度
                val = val * 2; 
                //限幅匹配 GUI 的量程 (4100)
                if(val > 4000) val = 4000; 
                
                pView->ChartData[i] = (int16_t)val;
            } else {
                pView->ChartData[i] = 0;
            }
        }
        pView->TrigStatus = TRIG_STATE_AUTO_RUN; 
    }
    else 
    {
        // === 示波器模式  ===
        int trigger_index = -1;
        
        // 查找上升沿
        for(int i=0; i < (ADC_SAMPLES_PER_BLOCK / 2); i++) {
            if(pRaw[i] < mean_val && pRaw[i+1] >= mean_val) {
                trigger_index = i; 
                break;
            }
        }
        
        if(trigger_index != -1) {
            pView->TrigStatus = TRIG_STATE_LOCKED;
            int copy_len = ADC_SAMPLES_PER_BLOCK - trigger_index;
            // 对齐拷贝
            for(int i=0; i<copy_len; i++) pView->ChartData[i] = pRaw[trigger_index + i];
            // 补齐
            for(int i=copy_len; i<CHART_POINTS; i++) pView->ChartData[i] = pRaw[ADC_SAMPLES_PER_BLOCK - 1]; 
        } else {
            pView->TrigStatus = (g_Config.ForceTrigMode == 0) ? TRIG_STATE_AUTO_RUN : TRIG_STATE_WAIT;
            // 直接拷贝
            for(int i=0; i<CHART_POINTS; i++) pView->ChartData[i] = pRaw[i];
        }
    }
}

static void SubTask_Update_Text(Scope_ViewData_t *pView)
{
    char status_str[16];
    
    if(g_Config.ViewMode == UI_MODE_PLAYBACK) {
        int pct = 0;
        if(s_TotalRecSize > 0) pct = (s_PlaybackOffset * 100) / s_TotalRecSize;
        snprintf(status_str, 16, "PLY %d%%", pct);
    }
    else if(g_Config.ViewMode == UI_MODE_FFT) strcpy(status_str, "FFT");
    else if(g_Config.AutoState == AUTO_FAIL) strcpy(status_str, "FAIL");
    else if(g_Config.AutoState != AUTO_IDLE && g_Config.AutoState != AUTO_LOCKED) strcpy(status_str, "SCAN");
    else if(pView->TrigStatus == TRIG_STATE_LOCKED) strcpy(status_str, "TRIG");
    else strcpy(status_str, "WAIT");

    char freq_str[16];
    if(pView->Freq < 1000) snprintf(freq_str, 16, "%d.%dHz", (int)pView->Freq, (int)((pView->Freq-(int)pView->Freq)*10));
    else snprintf(freq_str, 16, "%d.%02dk", (int)(pView->Freq/1000), (int)((pView->Freq/1000.0f - (int)(pView->Freq/1000))*100));

    snprintf(pView->Text_Info, 128,
        "RMS:%d.%02dV Freq:%s\n"
        "Max:%d.%d Min:%d.%d Duty:%d%%\n"
        "SR:%lu [%s]",
        (int)pView->Vrms, (int)((pView->Vrms-(int)pView->Vrms)*100),
        freq_str,
        (int)pView->Vmax, (int)((pView->Vmax-(int)pView->Vmax)*10),
        (int)pView->Vmin, (int)((pView->Vmin-(int)pView->Vmin)*10),
        pView->Duty,
        g_Config.SampleRate,
        status_str
    );
}

void Task_Scope_Core(void *pvParameters)
{
    uint32_t ulNotificationValue;
    uint16_t *pCurrentBuf;
    
    ADC_DMA_Init();
    ADC_DMA_Register_Task(xTaskGetCurrentTaskHandle());
    ADC_Set_Freq(g_Config.SampleRate);
    ADC_Start();

    while(1)
    {
        // 获取数据
        xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotificationValue, portMAX_DELAY);
        pCurrentBuf = &ADC_Raw_Buf[ulNotificationValue];
        
        // 准备后台缓冲
        uint8_t back_idx = g_front_idx ^ 1;
        Scope_ViewData_t *pNext = &g_ViewBuf[back_idx];
        // 插入录制
        Process_Recording_Stream(pCurrentBuf, ADC_SAMPLES_PER_BLOCK);

        // 统计计算
        q15_t raw_mean = SubTask_Calc_Stats(pCurrentBuf, pNext);

        // 频率计算
        SubTask_Calc_Freq(pCurrentBuf, pNext, raw_mean);
        
        // 波形/FFT 处理
        SubTask_Process_Wave(pCurrentBuf, pNext, raw_mean);

        // 更新 UI 文本
        SubTask_Update_Text(pNext);

        // 提交数据 (原子交换)
        __DSB();
        g_front_idx = back_idx;
    }
}