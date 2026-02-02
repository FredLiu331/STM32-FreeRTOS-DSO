#ifndef __DAC_WAVE_H
#define __DAC_WAVE_H
#include "stm32f10x.h"

#define DAC_DMA_CH    DMA2_Channel3

void DAC_Wave_Init(uint16_t *buffer, uint32_t len);
void DAC_Set_Sample_Rate(uint32_t freq_hz);
void DAC_Playback_Start(void);
void DAC_Playback_Stop(void);

#endif