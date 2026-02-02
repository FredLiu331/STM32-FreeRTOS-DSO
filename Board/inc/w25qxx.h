#ifndef __W25QXX_H
#define __W25QXX_H
#include "stm32f10x.h"

// CS   -> PB12
// SCK  -> PB13
// MISO -> PB14
// MOSI -> PB15
#define W25QXX_CS_PIN    GPIO_Pin_12
#define W25QXX_CS_PORT   GPIOB

// 片选控制宏
#define W25QXX_CS(x)     GPIO_WriteBit(W25QXX_CS_PORT, W25QXX_CS_PIN, (x ? Bit_SET : Bit_RESET))

// 常用指令
#define W25X_WriteEnable        0x06 
#define W25X_WriteDisable       0x04 
#define W25X_ReadStatusReg      0x05 
#define W25X_WriteStatusReg     0x01 
#define W25X_ReadData           0x03 
#define W25X_FastReadData       0x0B 
#define W25X_FastReadDual       0x3B 
#define W25X_PageProgram        0x02 
#define W25X_BlockErase64       0xD8 
#define W25X_SectorErase        0x20 
#define W25X_ChipErase          0xC7 
#define W25X_PowerDown          0xB9 
#define W25X_ReleasePowerDown   0xAB 
#define W25X_DeviceID           0xAB 
#define W25X_ManufactDeviceID   0x90 
#define W25X_JedecDeviceID      0x9F 

// 函数声明
void W25QXX_Init(void);
uint16_t W25QXX_ReadID(void);
uint8_t W25QXX_ReadWriteByte(uint8_t TxData);
void W25QXX_Read(uint8_t* pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead);
void W25QXX_Write_Page(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void W25QXX_Write_NoCheck(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void W25QXX_Erase_Sector(uint32_t Dst_Addr);

uint8_t W25QXX_Is_Busy(void);
void W25QXX_Write_Enable(void);
void W25QXX_Erase_Block64_NoWait(uint32_t Dst_Addr);
void W25QXX_Write_Page_NoWait(uint8_t* pBuffer, uint32_t WriteAddr);

#endif