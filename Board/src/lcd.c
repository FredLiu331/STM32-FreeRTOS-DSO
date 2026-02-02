#include "lcd.h"
// 官方的lcdex中的初始化序列
#define lcd_wr_regno(reg)  (LCD->LCD_REG = reg)
#define lcd_wr_data(data)  (LCD->LCD_RAM = data)
#define lcd_write_reg(reg, val) {LCD->LCD_REG = reg; LCD->LCD_RAM = val;}

uint16_t lcddev_width = 240;
uint16_t lcddev_height = 320;
uint16_t lcddev_id = 0;

void lcd_ex_st7789_reginit(void);
void LCD_WriteReg(uint16_t LCD_Reg, uint16_t LCD_RegValue)
{
    LCD->LCD_REG = LCD_Reg;
    LCD->LCD_RAM = LCD_RegValue;
}

uint16_t LCD_ReadReg(uint16_t LCD_Reg)
{
    LCD->LCD_REG = LCD_Reg;
    return LCD->LCD_RAM;
}

void LCD_WriteRAM_Prepare(void)
{
    LCD->LCD_REG = 0x2C; // 0x2C 是大部分 LCD 的写内存指令
}

void LCD_WriteRAM(uint16_t RGB_Code)
{
    LCD->LCD_RAM = RGB_Code;
}


static void delay_ms(uint32_t ms)
{
    uint32_t i, j;
    for(i = 0; i < ms; i++)
        for(j = 0; j < 10000; j++);
}

void LCD_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    FSMC_NORSRAMInitTypeDef FSMC_NORSRAMInitStructure;
    FSMC_NORSRAMTimingInitTypeDef readWriteTiming;
    FSMC_NORSRAMTimingInitTypeDef writeTiming;

    // 开启时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE | RCC_APB2Periph_GPIOG | RCC_APB2Periph_AFIO, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);

    // 配置背光 PB0
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_0); // 点亮背光

    // 配置 FSMC 数据线和控制线 (复用推挽输出)
    // PD0,1,4,5,8,9,10,14,15
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // PE7~15
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    // PG0 (A10 -> RS), PG12 (NE4 -> CS)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_12;
    GPIO_Init(GPIOG, &GPIO_InitStructure);

    // FSMC 参数配置
    readWriteTiming.FSMC_AddressSetupTime = 0x01; // 地址建立时间
    readWriteTiming.FSMC_AddressHoldTime = 0x00;  // 地址保持时间
    readWriteTiming.FSMC_DataSetupTime = 0x0f;    // 数据建立时间
    readWriteTiming.FSMC_BusTurnAroundDuration = 0x00;
    readWriteTiming.FSMC_CLKDivision = 0x00;
    readWriteTiming.FSMC_DataLatency = 0x00;
    readWriteTiming.FSMC_AccessMode = FSMC_AccessMode_A;

    writeTiming.FSMC_AddressSetupTime = 0x00;
    writeTiming.FSMC_AddressHoldTime = 0x00;
    writeTiming.FSMC_DataSetupTime = 0x03;
    writeTiming.FSMC_BusTurnAroundDuration = 0x00;
    writeTiming.FSMC_CLKDivision = 0x00;
    writeTiming.FSMC_DataLatency = 0x00;
    writeTiming.FSMC_AccessMode = FSMC_AccessMode_A;

    FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM4;
    FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
    FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_SRAM;
    FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
    FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
    FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
    FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;
    FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
    FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
    FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
    FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
    FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Enable;
    FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
    FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &readWriteTiming;
    FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &writeTiming;

    FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);
    FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM4, ENABLE);

    delay_ms(50); // 等待屏幕上电

    lcddev_id = 0x7789; 
    lcd_ex_st7789_reginit();
    
    // 设置扫描方向 (竖屏)
    LCD_WriteReg(0x36, 0x08);
    
    LCD_Clear(BLACK);
}

void LCD_Clear(uint16_t color)
{
    LCD_Fill(0, 0, lcddev_width - 1, lcddev_height - 1, color);
}

static void LCD_Set_Window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
    uint16_t ex = sx + width - 1;
    uint16_t ey = sy + height - 1;

    // 1. 设置列地址 (Column Address Set)
    LCD->LCD_REG = 0x2A; 
    LCD->LCD_RAM = sx >> 8;     // 起始 X 高8位
    LCD->LCD_RAM = sx & 0xFF;   // 起始 X 低8位
    LCD->LCD_RAM = ex >> 8;     // 结束 X 高8位
    LCD->LCD_RAM = ex & 0xFF;   // 结束 X 低8位

    // 2. 设置行地址 (Row Address Set)
    LCD->LCD_REG = 0x2B; 
    LCD->LCD_RAM = sy >> 8;     // 起始 Y 高8位
    LCD->LCD_RAM = sy & 0xFF;   // 起始 Y 低8位
    LCD->LCD_RAM = ey >> 8;     // 结束 Y 高8位
    LCD->LCD_RAM = ey & 0xFF;   // 结束 Y 低8位
}

void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos)
{
    LCD_Set_Window(Xpos, Ypos, 1, 1);
}

// 区域填充颜色
// 此函数会被 lv_port_disp.c 调用
void LCD_Color_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    uint16_t width = ex - sx + 1;
    uint16_t height = ey - sy + 1;
    uint32_t i;
    uint32_t total = (uint32_t)width * height;

    LCD_Set_Window(sx, sy, width, height);

    LCD->LCD_REG = 0x2C; 

    for(i = 0; i < total; i++)
    {
        LCD->LCD_RAM = *color++;
    }
}

void LCD_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    uint16_t width = ex - sx + 1;
    uint16_t height = ey - sy + 1;
    uint32_t i;
    uint32_t total = (uint32_t)width * height;

    // 1. 设置窗口
    LCD_Set_Window(sx, sy, width, height);

    // 2. 发送写内存指令
    LCD->LCD_REG = 0x2C; 

    // 3. 连续写入颜色数据
    for (i = 0; i < total; i++)
    {
        LCD->LCD_RAM = color;
    }
}

void lcd_ex_st7789_reginit(void)
{
    lcd_wr_regno(0x11);

    delay_ms(120); 

    lcd_wr_regno(0x36);
    lcd_wr_data(0x00);


    lcd_wr_regno(0x3A);
    lcd_wr_data(0X05);

    lcd_wr_regno(0xB2);
    lcd_wr_data(0x0C);
    lcd_wr_data(0x0C);
    lcd_wr_data(0x00);
    lcd_wr_data(0x33);
    lcd_wr_data(0x33);

    lcd_wr_regno(0xB7);
    lcd_wr_data(0x35);

    lcd_wr_regno(0xBB); /* vcom */
    lcd_wr_data(0x32);  /* 30 */

    lcd_wr_regno(0xC0);
    lcd_wr_data(0x0C);

    lcd_wr_regno(0xC2);
    lcd_wr_data(0x01);

    lcd_wr_regno(0xC3); /* vrh */
    lcd_wr_data(0x10);  /* 17 0D */

    lcd_wr_regno(0xC4); /* vdv */
    lcd_wr_data(0x20);  /* 20 */

    lcd_wr_regno(0xC6);
    lcd_wr_data(0x0f);

    lcd_wr_regno(0xD0);
    lcd_wr_data(0xA4); 
    lcd_wr_data(0xA1); 

    lcd_wr_regno(0xE0); /* Set Gamma  */
    lcd_wr_data(0xd0);
    lcd_wr_data(0x00);
    lcd_wr_data(0x02);
    lcd_wr_data(0x07);
    lcd_wr_data(0x0a);
    lcd_wr_data(0x28);
    lcd_wr_data(0x32);
    lcd_wr_data(0X44);
    lcd_wr_data(0x42);
    lcd_wr_data(0x06);
    lcd_wr_data(0x0e);
    lcd_wr_data(0x12);
    lcd_wr_data(0x14);
    lcd_wr_data(0x17);


    lcd_wr_regno(0XE1);  /* Set Gamma */
    lcd_wr_data(0xd0);
    lcd_wr_data(0x00);
    lcd_wr_data(0x02);
    lcd_wr_data(0x07);
    lcd_wr_data(0x0a);
    lcd_wr_data(0x28);
    lcd_wr_data(0x31);
    lcd_wr_data(0x54);
    lcd_wr_data(0x47);
    lcd_wr_data(0x0e);
    lcd_wr_data(0x1c);
    lcd_wr_data(0x17);
    lcd_wr_data(0x1b); 
    lcd_wr_data(0x1e);


    lcd_wr_regno(0x2A);
    lcd_wr_data(0x00);
    lcd_wr_data(0x00);
    lcd_wr_data(0x00);
    lcd_wr_data(0xef);

    lcd_wr_regno(0x2B);
    lcd_wr_data(0x00);
    lcd_wr_data(0x00);
    lcd_wr_data(0x01);
    lcd_wr_data(0x3f);

    lcd_wr_regno(0x29); /* display on */
}
