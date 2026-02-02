#ifndef __KEY_H
#define __KEY_H
#include "stm32f10x.h"

#define KEY0_PIN    GPIO_Pin_4
#define KEY0_PORT   GPIOE
#define KEY1_PIN    GPIO_Pin_3
#define KEY1_PORT   GPIOE

#define KEY0_READ   (GPIO_ReadInputDataBit(KEY0_PORT, KEY0_PIN) == 0)
#define KEY1_READ   (GPIO_ReadInputDataBit(KEY1_PORT, KEY1_PIN) == 0)

void KEY_Init(void);
uint8_t KEY_Scan(void);

#endif