#ifndef __LCD_H
#define __LCD_H
#include "stm32f10x.h"

#define L2R_U2D  0
#define DFT_SCAN_DIR  L2R_U2D

#define WHITE   0xFFFF
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0

typedef struct
{
    volatile uint16_t LCD_REG;
    volatile uint16_t LCD_RAM;
} LCD_TypeDef;

#define LCD_BASE        ((uint32_t)(0x6C000000 | 0x000007FE))
#define LCD             ((LCD_TypeDef *) LCD_BASE)

extern uint16_t lcddev_width;
extern uint16_t lcddev_height;
extern uint16_t lcddev_id;

void LCD_Init(void);
void LCD_Clear(uint16_t color);
void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos);
void LCD_WriteReg(uint16_t LCD_Reg, uint16_t LCD_RegValue);
void LCD_WriteRAM_Prepare(void);
void LCD_WriteRAM(uint16_t RGB_Code);
void LCD_Color_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color); // LVGL专用
void LCD_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);
uint16_t LCD_ReadReg(uint16_t LCD_Reg);
#endif