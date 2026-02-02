#include "led.h"

void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(LED0_CLK | LED1_CLK, ENABLE);

    GPIO_InitStructure.GPIO_Pin = LED0_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED0_PORT, &GPIO_InitStructure);
    GPIO_SetBits(LED0_PORT, LED0_PIN);

    // 3. 配置 LED1 (PE5)
    GPIO_InitStructure.GPIO_Pin = LED1_PIN;
    GPIO_Init(LED1_PORT, &GPIO_InitStructure);
    GPIO_SetBits(LED1_PORT, LED1_PIN);
}


void LED_Set(uint8_t state)
{
    if(state)
    {
        LED0_ON(); // 亮红灯
    }
    else
    {
        LED0_OFF(); // 灭红灯
    }
}