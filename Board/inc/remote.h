#ifndef __REMOTE_H
#define __REMOTE_H

#include "stm32f10x.h"

#define REMOTE_POWER    0x45
#define REMOTE_MENU     0x47
#define REMOTE_TEST     0x44
#define REMOTE_BACK     0x43
#define REMOTE_PLAY     0x15
#define REMOTE_PLUS     0x40
#define REMOTE_MINUS    0x19
#define REMOTE_RIGHT    0x09
#define REMOTE_LEFT     0x07
#define REMOTE_C        0x0D
#define REMOTE_0        0x16
#define REMOTE_1        0x0c
#define REMOTE_2        0x18
#define REMOTE_3        0x5E
#define REMOTE_4        0x08
#define REMOTE_5        0x1C
#define REMOTE_6        0x5A
#define REMOTE_7        0x42
#define REMOTE_8        0x52
#define REMOTE_9        0x4A

extern volatile uint8_t g_RemoteReady;
extern volatile uint8_t g_RemoteKey;

void Remote_Init(void);
uint8_t Remote_Scan(void);

#endif