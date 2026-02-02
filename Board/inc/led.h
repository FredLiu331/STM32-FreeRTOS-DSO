#ifndef __LED_H
#define __LED_H
#include "stm32f10x.h"

// LED0 -> PB5 (红色)
#define LED0_PIN        GPIO_Pin_5
#define LED0_PORT       GPIOB
#define LED0_CLK        RCC_APB2Periph_GPIOB

// LED1 -> PE5 (绿色)
#define LED1_PIN        GPIO_Pin_5
#define LED1_PORT       GPIOE
#define LED1_CLK        RCC_APB2Periph_GPIOE

// 控制宏 (低电平亮，高电平灭)
#define LED0(x)   GPIO_WriteBit(LED0_PORT, LED0_PIN, (x) ? Bit_RESET : Bit_SET)
#define LED1(x)   GPIO_WriteBit(LED1_PORT, LED1_PIN, (x) ? Bit_RESET : Bit_SET)

// 简易操作宏
#define LED0_ON()    GPIO_ResetBits(LED0_PORT, LED0_PIN)
#define LED0_OFF()   GPIO_SetBits(LED0_PORT, LED0_PIN)
#define LED0_TOGGLE()  {LED0_PORT->ODR ^= LED0_PIN;}

#define LED1_ON()    GPIO_ResetBits(LED1_PORT, LED1_PIN)
#define LED1_OFF()   GPIO_SetBits(LED1_PORT, LED1_PIN)
#define LED1_TOGGLE()  {LED1_PORT->ODR ^= LED1_PIN;}

// 函数声明
void LED_Init(void);
void LED_Set(uint8_t state); // 兼容 main.c 的控制函数

#endif