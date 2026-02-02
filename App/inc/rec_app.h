#ifndef __REC_APP_H
#define __REC_APP_H

#include <stdint.h>

#define REC_FLASH_START     0x001000        // 起始地址
#define REC_MAX_SIZE        (2 * 1024 * 1024) // 最大录制 2MB
void Rec_Controller_Start(void);
void Rec_Controller_Stop(void);

// 数据接口 (供 ScopeCore 调用)
// 返回 1=成功, 0=队列满丢帧
int Rec_Push_Data(uint16_t *data_buf, uint16_t len);

// 状态查询 (供 GUI 使用)
const char* Rec_Get_State_String(void);
uint32_t Rec_Get_Recorded_Len(void); // 已录制长度(KB)

// 任务入口
void Task_Recorder(void *pvParameters);

#endif