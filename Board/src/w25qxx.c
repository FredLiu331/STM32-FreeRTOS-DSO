#include "w25qxx.h"

// SPI2 读写一个字节
uint8_t SPI2_ReadWriteByte(uint8_t TxData)
{
    uint16_t retry = 0;
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET)
    {
        retry++;
        if(retry > 20000) return 0;
    }
    SPI_I2S_SendData(SPI2, TxData);
    
    retry = 0;
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET)
    {
        retry++;
        if(retry > 20000) return 0;
    }
    return SPI_I2S_ReceiveData(SPI2);
}

void W25QXX_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;

    // 开启时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2,  ENABLE);

    // 配置 GPIO
    // PB13(SCK), PB15(MOSI) -> 复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // PB14(MISO) -> 浮空输入 (或带上拉输入)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // PB12(CS) -> 推挽输出 (软件控制片选)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    W25QXX_CS(1); // 取消片选

    // 配置 SPI2
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    // 模式 3
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High; 
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge; 
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft; // 软件 NSS
    // SPI2 在 APB1 (36MHz)，分频 2 -> 18MHz
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2; 
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &SPI_InitStructure);

    SPI_Cmd(SPI2, ENABLE);
}

// W25Q128 的 ID 应该是 0xEF17 (厂商EF, 容量17)
uint16_t W25QXX_ReadID(void)
{
    uint16_t Temp = 0;
    W25QXX_CS(0); // 选中
    SPI2_ReadWriteByte(W25X_ManufactDeviceID); // 发送命令 0x90
    SPI2_ReadWriteByte(0x00); // 发送 24bit 地址 000000
    SPI2_ReadWriteByte(0x00);
    SPI2_ReadWriteByte(0x00);
    Temp |= SPI2_ReadWriteByte(0xFF) << 8;
    Temp |= SPI2_ReadWriteByte(0xFF);
    W25QXX_CS(1); // 取消选中
    return Temp;
}

// 读取数据
void W25QXX_Read(uint8_t* pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead)
{
    uint16_t i;
    W25QXX_CS(0);
    SPI2_ReadWriteByte(W25X_ReadData); // 发送读取命令
    SPI2_ReadWriteByte((uint8_t)((ReadAddr) >> 16)); // 发送 24bit 地址
    SPI2_ReadWriteByte((uint8_t)((ReadAddr) >> 8));
    SPI2_ReadWriteByte((uint8_t)ReadAddr);
    for(i = 0; i < NumByteToRead; i++)
    {
        pBuffer[i] = SPI2_ReadWriteByte(0xFF); // 循环读
    }
    W25QXX_CS(1);
}


// 写使能 (每次写入/擦除前必须调用)
void W25QXX_Write_Enable(void)
{
    W25QXX_CS(0);
    SPI2_ReadWriteByte(W25X_WriteEnable);
    W25QXX_CS(1);
}

// 等待芯片空闲 (写入/擦除后必须等待)
// Flash 内部操作很慢，需要不断查询状态寄存器 SR1 的 BUSY 位 (Bit0)
void W25QXX_Wait_Busy(void)
{
    uint8_t status = 0;
    do
    {
        W25QXX_CS(0);
        SPI2_ReadWriteByte(W25X_ReadStatusReg); // 发送命令 05h
        status = SPI2_ReadWriteByte(0xFF);      // 读取状态
        W25QXX_CS(1);
    }
    while ((status & 0x01) == 0x01); // 如果 Bit0=1，说明正在忙，死循环等待
}

// 擦除一个扇区 (Sector = 4KB)
void W25QXX_Erase_Sector(uint32_t Dst_Addr)
{
    W25QXX_Write_Enable();  // 1. 写使能
    W25QXX_Wait_Busy();     // 2. 确保之前没事做

    W25QXX_CS(0);
    SPI2_ReadWriteByte(W25X_SectorErase);      // 3. 发送擦除命令 0x20
    SPI2_ReadWriteByte((uint8_t)((Dst_Addr) >> 16)); // 发送24位地址
    SPI2_ReadWriteByte((uint8_t)((Dst_Addr) >> 8));
    SPI2_ReadWriteByte((uint8_t)Dst_Addr);
    W25QXX_CS(1);

    W25QXX_Wait_Busy();     // 4. 等待擦除完成
}


void W25QXX_Write_Page(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    uint16_t i;
    W25QXX_Write_Enable();  // 1. 写使能
    
    W25QXX_CS(0);
    SPI2_ReadWriteByte(W25X_PageProgram);      // 2. 发送页编程命令 0x02
    SPI2_ReadWriteByte((uint8_t)((WriteAddr) >> 16)); // 发送地址
    SPI2_ReadWriteByte((uint8_t)((WriteAddr) >> 8));
    SPI2_ReadWriteByte((uint8_t)WriteAddr);
    for(i = 0; i < NumByteToWrite; i++)
    {
        SPI2_ReadWriteByte(pBuffer[i]);        // 3. 发送数据
    }
    W25QXX_CS(1);

    W25QXX_Wait_Busy();     // 4. 等待写入结束
}

void W25QXX_Write_NoCheck(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    uint16_t pageremain;
    pageremain = 256 - (WriteAddr % 256); // 当前页还剩多少字节可写

    if(NumByteToWrite <= pageremain) pageremain = NumByteToWrite; // 如果要写的数据很少，不够填满当前页

    while(1)
    {
        W25QXX_Write_Page(pBuffer, WriteAddr, pageremain); // 写一页(或不足一页)
        
        if(NumByteToWrite == pageremain) break; // 写完了
        else // 没写完
        {
            pBuffer += pageremain;         // 指针后移
            WriteAddr += pageremain;       // 地址后移
            NumByteToWrite -= pageremain;  // 剩余字节数减少
            if(NumByteToWrite > 256) pageremain = 256; // 下一次写满一页
            else pageremain = NumByteToWrite;          // 下一次写剩下的
        }
    }
}

uint8_t W25QXX_Is_Busy(void)
{
    uint8_t status;
    W25QXX_CS(0);
    SPI2_ReadWriteByte(W25X_ReadStatusReg);
    status = SPI2_ReadWriteByte(0xFF);
    W25QXX_CS(1);
    return (status & 0x01);
}

void W25QXX_Erase_Block64_NoWait(uint32_t Dst_Addr)
{
    W25QXX_Write_Enable(); // 必须先写使能
    
    W25QXX_CS(0);
    SPI2_ReadWriteByte(W25X_BlockErase64); // 0xD8
    SPI2_ReadWriteByte((uint8_t)((Dst_Addr) >> 16));
    SPI2_ReadWriteByte((uint8_t)((Dst_Addr) >> 8));
    SPI2_ReadWriteByte((uint8_t)Dst_Addr);
    W25QXX_CS(1);
    // 注意：这里不等待 Busy，直接返回！
}

void W25QXX_Write_Page_NoWait(uint8_t* pBuffer, uint32_t WriteAddr)
{
    W25QXX_Write_Enable();
    
    W25QXX_CS(0);
    SPI2_ReadWriteByte(W25X_PageProgram); // 0x02
    SPI2_ReadWriteByte((uint8_t)((WriteAddr) >> 16));
    SPI2_ReadWriteByte((uint8_t)((WriteAddr) >> 8));
    SPI2_ReadWriteByte((uint8_t)WriteAddr);
    
    // 循环发送 256 字节
    for(int i=0; i<256; i++)
    {
        SPI2_ReadWriteByte(pBuffer[i]);
    }
    W25QXX_CS(1);
}