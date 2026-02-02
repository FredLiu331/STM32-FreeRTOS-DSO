#include "key.h"

void KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);

    GPIO_InitStructure.GPIO_Pin = KEY0_PIN | KEY1_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 上拉输入
    GPIO_Init(GPIOE, &GPIO_InitStructure);
}

uint8_t KEY_Scan(void)
{
    static uint8_t key_up = 1; // 按键松开标志
    
    if (key_up && (KEY0_READ == 0 || KEY1_READ == 0))
    {
        key_up = 0;
        if (KEY0_READ) return 1;
        if (KEY1_READ) return 2;
    }
    else if (!KEY0_READ && !KEY1_READ)
    {
        key_up = 1;
    }
    return 0;
}