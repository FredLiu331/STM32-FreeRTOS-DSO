#include "dac.h"

static uint32_t g_wave_len = 0;

void DAC_Wave_Init(uint16_t *buffer, uint32_t len)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    DAC_InitTypeDef  DAC_InitStructure;
    DMA_InitTypeDef  DMA_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    g_wave_len = len;

    // 开启时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);      // PA4
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC | RCC_APB1Periph_TIM6, ENABLE); // DAC + TIM6
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);         // DMA2 (DAC_CH1 只能用 DMA2_Ch3)

    // 配置 GPIO
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. 配置 TIM6
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = 720 - 1;       // 默认初值，后续由 Set_Sample_Rate 修改
    TIM_TimeBaseStructure.TIM_Prescaler = 10 - 1;     
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM6, &TIM_TimeBaseStructure);
    TIM_SelectOutputTrigger(TIM6, TIM_TRGOSource_Update);

    // 配置 DAC
    DAC_StructInit(&DAC_InitStructure);
    DAC_InitStructure.DAC_Trigger = DAC_Trigger_T6_TRGO; // 硬件触发源选 TIM6
    DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None; // 不生成噪声/三角波，我们要自定义波形
    DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Enable;   // 开启输出缓冲，增加驱动能力
    DAC_Init(DAC_Channel_1, &DAC_InitStructure);

    // 配置 DMA2_Channel3
    DMA_DeInit(DAC_DMA_CH);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&DAC->DHR12R1; // 目标：DAC 12位右对齐数据寄存器
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)buffer;            // 源：内存数组
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;                  // 方向：内存 -> 外设
    DMA_InitStructure.DMA_BufferSize = len;                             // 数据量
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;    // 外设地址不增
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;             // 内存地址递增
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; // 16位 (DAC寄存器是16位)
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;         // 16位 (数组是 u16)
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;                     // 循环模式 (波形循环播放)
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DAC_DMA_CH, &DMA_InitStructure);

    // 开启各模块连接
    DAC_Cmd(DAC_Channel_1, ENABLE);          // 使能 DAC
    // DAC_SetChannel1Data( DAC_Align_12b_R, 2048);
    DAC_DMACmd(DAC_Channel_1, ENABLE);       // 使能 DAC 的 DMA 请求
    DMA_Cmd(DAC_DMA_CH, ENABLE);             // 使能 DMA 通道
    TIM_Cmd(TIM6, ENABLE);                   // 启动定时器
}

void DAC_Set_Sample_Rate(uint32_t freq_hz)
{
    uint16_t psc = 0;
    uint32_t arr = 0;

    if(freq_hz == 0) freq_hz = 1;

    // 如果频率很高 (>1kHz)，PSC=0 (不分频)，调节 ARR
    // 72,000,000 / (ARR+1) = Freq
    // ARR = 72M/Freq - 1
    if(freq_hz < 1100) // 72M/65535 ≈ 1098
    {
        psc = 7200 - 1; // 预分频 7200 -> 10kHz 计数频率
        arr = (10000 / freq_hz) - 1;
    }
    else
    {
        psc = 0;
        arr = (72000000 / freq_hz) - 1;
    }

    TIM_PrescalerConfig(TIM6, psc, TIM_PSCReloadMode_Immediate);
    TIM_SetAutoreload(TIM6, arr);
}

void DAC_Playback_Stop(void)
{
    TIM_Cmd(TIM6, DISABLE); // 停掉定时器，DAC 就不会收到触发信号，输出保持在最后一个电平
}

void DAC_Playback_Start(void)
{
    TIM_Cmd(TIM6, ENABLE);
}