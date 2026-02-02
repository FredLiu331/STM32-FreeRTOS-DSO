#include "sys_monitor.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

// 全局变量
volatile uint32_t g_IdleCount = 0;      
static uint32_t g_IdleCountMax = 0;     // 历史最大空闲计数值
static float g_CpuUsage = 0.0f;

// 缓冲区
static char g_SysInfoCache[512] = "Wait...";

// 钩子函数
void vApplicationIdleHook(void) { 
    g_IdleCount++; 
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    taskDISABLE_INTERRUPTS();
    for(;;);
}

// 字符串工具
static char* Append_Num(char* p, uint32_t num, uint8_t width) {
    char temp[10];
    int i = 0;
    if(num == 0) temp[i++] = '0';
    else while(num > 0) { temp[i++] = (num % 10) + '0'; num /= 10; }
    while(i < width) { *p++ = ' '; width--; }
    while(i > 0) { *p++ = temp[--i]; }
    return p;
}

static char* Append_Task_Item(char* p, const char* name, uint32_t stack_left) {
    int len = 0;
    // 限制任务名长度为 4
    while(*name && len < 4) { *p++ = *name++; len++; }
    while(len < 5) { *p++ = ' '; len++; } 
    p = Append_Num(p, stack_left, 3);
    return p;
}


// 核心逻辑
static void Update_Sys_Info_Double_Col(void)
{
    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize, x;
    uint32_t ulTotalRunTime;
    char *pPtr = g_SysInfoCache;

    uxArraySize = uxTaskGetNumberOfTasks();
    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

    if (pxTaskStatusArray != NULL)
    {
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

        // --- CPU ---
        uint32_t cpu_val = (uint32_t)(g_CpuUsage * 10);
        strcpy(pPtr, "CPU:"); pPtr += 4;
        pPtr = Append_Num(pPtr, cpu_val / 10, 2); 
        *pPtr++ = '.';
        pPtr = Append_Num(pPtr, cpu_val % 10, 1);
        *pPtr++ = '%';
        *pPtr++ = ' '; 

        size_t free_heap = xPortGetFreeHeapSize();
        strcpy(pPtr, " Heap:"); pPtr += 6;
        pPtr = Append_Num(pPtr, free_heap / 1024, 2); // 显示 KB
        *pPtr++ = 'K';
        *pPtr++ = '\n';

        // --- 任务列表双列 ---
        for (x = 0; x < uxArraySize; x++)
        {
            if ((pPtr - g_SysInfoCache) > 480) break;

            pPtr = Append_Task_Item(pPtr, pxTaskStatusArray[x].pcTaskName, pxTaskStatusArray[x].usStackHighWaterMark);

            if (x % 2 == 0) {
                strcpy(pPtr, "  "); pPtr += 2; 
            } else {
                *pPtr++ = '\n';
            }
        }
        *pPtr = '\0';
        vPortFree(pxTaskStatusArray);
    }
}

// 任务主体
void Task_SysMonitor(void *pvParameters)
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 500; // 【修改点】改为 500ms
    
    // 初始化
    g_IdleCount = 0;
    g_IdleCountMax = 0; 
    xLastWakeTime = xTaskGetTickCount();

    while(1)
    {
        // 1. 重置计数
        g_IdleCount = 0;
        
        // 2. 等待 500ms
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // 只要发现现在的空闲计数比历史记录还大，说明系统更闲了，更新基准值
        if (g_IdleCount > g_IdleCountMax) {
            g_IdleCountMax = g_IdleCount;
        }

        // 4. 计算 CPU
        if (g_IdleCountMax > 0) {
            // 简单的防止溢出保护
            uint32_t current_idle = g_IdleCount;
            if(current_idle > g_IdleCountMax) current_idle = g_IdleCountMax;
            
            // 计算公式
            g_CpuUsage = 100.0f - ((float)current_idle * 100.0f / g_IdleCountMax);
        }

        // 5. 生成文本
        Update_Sys_Info_Double_Col();
    }
}

// void Sys_Monitor_Init(void) {
//     xTaskCreate(Task_SysMonitor, "Monitor", 128, NULL, 1, NULL);
// }

void Sys_Get_Info_Text(char *buffer, uint16_t max_len) {
    strncpy(buffer, g_SysInfoCache, max_len);
    buffer[max_len - 1] = '\0';
}