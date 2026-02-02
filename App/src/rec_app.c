#include "rec_app.h"
#include "w25qxx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "gui_app.h" // 用于 Log
#include <string.h>

// --- 配置参数 ---
#define ERASE_BLOCK_SIZE    65536           // 64KB Block
#define PAGE_SIZE           256             // Page Program 大小

// 缓冲区配置
#define QUEUE_DEPTH         40
#define PACKET_SIZE         256 // 字节

// 数据包定义
typedef struct {
    uint8_t raw[PACKET_SIZE];
} RecPacket_t;

// 内部状态机
typedef enum {
    STATE_IDLE,
    STATE_BUSY_WAIT,    // 等待 Flash 忙碌结束
    STATE_DECIDE,       // 决策：擦除 还是 写入
    STATE_STOPPING      // 正在停止
} InnerState_t;

// 全局变量
static volatile uint8_t g_IsRecording = 0; // 外部开关
static QueueHandle_t xRecQueue = NULL;
static InnerState_t g_InnerState = STATE_IDLE;

// 进度追踪
static uint32_t g_WriteAddr = 0;  // 当前写到哪
static uint32_t g_EraseAddr = 0;  // 当前擦到哪

// ---------------- 外部接口 ----------------

void Rec_Controller_Start(void) {
    if(!g_IsRecording) {
        g_IsRecording = 1; // 开启标志
        // 状态机复位逻辑在 Task 中处理
    }
}

void Rec_Controller_Stop(void) {
    g_IsRecording = 0; // 关闭标志
}

const char* Rec_Get_State_String(void) {
    if(g_InnerState == STATE_IDLE) return "IDLE";
    if(g_IsRecording) return "REC";
    return "STOP";
}

uint32_t Rec_Get_Recorded_Len(void) {
    if(g_WriteAddr < REC_FLASH_START) return 0;
    return (g_WriteAddr - REC_FLASH_START) / 1024; // KB
}

// 生产者接口 (ScopeCore 调用)
int Rec_Push_Data(uint16_t *data_buf, uint16_t len)
{
    // 如果没在录制，直接丢弃
    if (g_InnerState == STATE_IDLE || g_InnerState == STATE_STOPPING) return 1;
    if (xRecQueue == NULL) return 0;

    RecPacket_t pkt;
    // 将 uint16 数组转为 uint8 字节流存入包中
    memcpy(pkt.raw, data_buf, len * 2);

    // 推送队列 (不等待，满了就丢)
    if(xQueueSend(xRecQueue, &pkt, 0) != pdPASS) {
        return 0; // 丢帧
    }
    return 1;
}

// ---------------- 消费者任务 ----------------

void Task_Recorder(void *pvParameters)
{
    RecPacket_t current_pkt;
    
    // 创建大容量缓冲队列
    xRecQueue = xQueueCreate(QUEUE_DEPTH, sizeof(RecPacket_t));
    if (xRecQueue == NULL) {
        GUI_Log("ERR: Rec Queue Fail!");
        vTaskDelete(NULL); // 自杀，不跑后面的逻辑
        return;
    }
    W25QXX_Init();

    GUI_Log("Recorder Task Ready.");

    while(1)
    {
        switch(g_InnerState)
        {
            // === 空闲态 ===
            case STATE_IDLE:
                if(g_IsRecording) {
                    GUI_Log("Rec: Starting...");
                    g_WriteAddr = REC_FLASH_START;
                    W25QXX_Erase_Block64_NoWait(REC_FLASH_START);
                    g_EraseAddr = REC_FLASH_START + ERASE_BLOCK_SIZE;
                    
                    xQueueReset(xRecQueue); // 清空旧数据
                    g_InnerState = STATE_BUSY_WAIT; // 等待擦除完成
                } else {
                    vTaskDelay(100); // 没活干就睡觉
                }
                break;

            // === 忙碌等待态 ===
            case STATE_BUSY_WAIT:
                if(W25QXX_Is_Busy()) {
                    // Flash 忙时，挂起任务 1ms
                    // 实现了“后台擦除”的效果
                    vTaskDelay(1); 
                } else {
                    // Flash 空闲了，去决定下一步干啥
                    g_InnerState = STATE_DECIDE;
                }
                break;

            // === 决策态 ===
            case STATE_DECIDE:
                // 1. 检查停止信号
                if(!g_IsRecording) {
                    g_InnerState = STATE_STOPPING;
                    break;
                }
                
                // 2. 检查存储空间是否满
                if(g_WriteAddr >= REC_FLASH_START + REC_MAX_SIZE) {
                    GUI_Log("Rec: Storage Full!");
                    g_IsRecording = 0;
                    g_InnerState = STATE_STOPPING;
                    break;
                }

                // 3.预擦除逻辑
                // 如果当前写指针进入了已擦除区域的最后 4KB
                // 必须赶紧擦除下一个 64KB 块
                if (g_WriteAddr + 4096 >= g_EraseAddr) {
                    // 发起异步块擦除
                    W25QXX_Erase_Block64_NoWait(g_EraseAddr);
                    g_EraseAddr += ERASE_BLOCK_SIZE;
                    // 转入忙碌等待，此时数据会堆积在 Queue 中
                    g_InnerState = STATE_BUSY_WAIT; 
                    break;
                }

                // 4.写入逻辑
                if(xQueueReceive(xRecQueue, &current_pkt, 0) == pdTRUE) {
                    // 发起页编程 (256字节)
                    W25QXX_Write_Page_NoWait(current_pkt.raw, g_WriteAddr);
                    g_WriteAddr += PAGE_SIZE;
                    // 写完指令后，Flash 也会忙几百微秒
                    g_InnerState = STATE_BUSY_WAIT;
                } else {
                    // 队列空了，也没有擦除任务，休息一下
                    vTaskDelay(1);
                }
                break;

            // === 停止处理 ===
            case STATE_STOPPING:
                GUI_Log("Rec: Stopped. Size: %d KB", (g_WriteAddr - REC_FLASH_START)/1024);
                g_InnerState = STATE_IDLE;
                break;
        }
    }
}