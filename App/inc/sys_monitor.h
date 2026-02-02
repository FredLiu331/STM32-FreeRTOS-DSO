#ifndef __SYS_MONITOR_H
#define __SYS_MONITOR_H

#include <stdint.h>

// 初始化监控任务
void Task_SysMonitor(void *pvParameters);

// 获取格式化后的系统状态 (CPU + 堆栈信息)
void Sys_Get_Info_Text(char *buffer, uint16_t max_len);

#endif