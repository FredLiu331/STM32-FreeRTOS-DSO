#ifndef __ADC_DMA_H
#define __ADC_DMA_H
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

#define ADC_SAMPLES_PER_BLOCK   256             // 单次处理的数据块大小
#define ADC_DMA_BUF_SIZE        (ADC_SAMPLES_PER_BLOCK * 2) // 双缓冲总大小

extern uint16_t ADC_Raw_Buf[ADC_DMA_BUF_SIZE];

void ADC_DMA_Init(void);

void ADC_DMA_Register_Task(TaskHandle_t handle);

void ADC_Set_Freq(uint32_t freq_hz);
void ADC_Start(void);
void ADC_Stop(void);

#endif