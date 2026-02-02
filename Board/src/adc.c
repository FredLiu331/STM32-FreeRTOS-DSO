#include "adc.h"

// 双缓冲数组 (32位对齐)
__attribute__((aligned(4))) uint16_t ADC_Raw_Buf[ADC_DMA_BUF_SIZE];

// 保存需要通知的任务句柄
static TaskHandle_t s_TargetTask = NULL;

void ADC_DMA_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef  ADC_InitStructure;
    DMA_InitTypeDef  DMA_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 时钟使能
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE); 
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6); 

    // GPIO PA1
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // TIM3 (触发源)
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = 1000 - 1; 
    TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1; 
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
    TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update);

    // DMA1_Channel1
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)ADC_Raw_Buf;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = ADC_DMA_BUF_SIZE;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular; // 循环模式
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    
    // 开启中断
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC | DMA_IT_HT, ENABLE);

    // NVIC
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // ADC1
    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T3_TRGO;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_1Cycles5);

    // 使能
    DMA_Cmd(DMA1_Channel1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);
    
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));

    ADC_ExternalTrigConvCmd(ADC1, ENABLE);
    ADC_DMACmd(ADC1, ENABLE);
    TIM_Cmd(TIM3, DISABLE); // 默认不启动
}

void ADC_DMA_Register_Task(TaskHandle_t handle)
{
    s_TargetTask = handle;
}

void ADC_Set_Freq(uint32_t freq_hz)
{
    if(freq_hz == 0) freq_hz = 1;
    TIM_Cmd(TIM3, DISABLE);
    
    uint16_t psc = 0;
    uint32_t arr = 0;
    
    if(freq_hz < 2000) {
        psc = 7200 - 1; 
        arr = (10000 / freq_hz) - 1;
    } else {
        psc = 0; 
        arr = (72000000 / freq_hz) - 1;
    }
    
    TIM_PrescalerConfig(TIM3, psc, TIM_PSCReloadMode_Immediate);
    TIM_SetAutoreload(TIM3, arr);
    TIM_Cmd(TIM3, ENABLE);
}

void ADC_Start(void) { TIM_Cmd(TIM3, ENABLE); }
void ADC_Stop(void)  { TIM_Cmd(TIM3, DISABLE); }

void DMA1_Channel1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(DMA_GetITStatus(DMA1_IT_HT1)) // 半满
    {
        DMA_ClearITPendingBit(DMA1_IT_HT1);
        if(s_TargetTask != NULL)
            xTaskNotifyFromISR(s_TargetTask, 0, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
    }
    
    if(DMA_GetITStatus(DMA1_IT_TC1)) // 全满
    {
        DMA_ClearITPendingBit(DMA1_IT_TC1);
        if(s_TargetTask != NULL)
            xTaskNotifyFromISR(s_TargetTask, ADC_SAMPLES_PER_BLOCK, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}