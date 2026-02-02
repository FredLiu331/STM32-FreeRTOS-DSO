#include "remote.h"
#include "misc.h"

typedef enum {
    RMT_IDLE = 0,   // 空闲
    RMT_HEADER,     // 收到引导码(9ms低+4.5ms高)
    RMT_DATA        // 接收数据位
} RemoteState_t;

volatile uint8_t g_RemoteReady = 0;
volatile uint8_t g_RemoteKey = 0;

static volatile RemoteState_t s_RmtState = RMT_IDLE;
static volatile uint32_t s_RmtData = 0;
static volatile uint8_t  s_RmtBitCnt = 0;

void Remote_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_ICInitTypeDef  TIM_ICInitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // GPIO PB9 输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_9);

    // TIM4 时基
    TIM_TimeBaseStructure.TIM_Period = 50000;
    TIM_TimeBaseStructure.TIM_Prescaler = (72 - 1); // 72MHz / 72 = 1MHz (1us)
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    // 输入捕获配置
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_4;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Falling; // 下降沿捕获
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0x03; // 滤波
    TIM_ICInit(TIM4, &TIM_ICInitStructure);
    
    // 中断
    NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6; // 低于示波器核心任务
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_ITConfig(TIM4, TIM_IT_Update | TIM_IT_CC4, ENABLE);
    TIM_Cmd(TIM4, ENABLE);
}

// 获取键值
uint8_t Remote_Scan(void)
{
    if(g_RemoteReady) {
        uint8_t key = g_RemoteKey;
        g_RemoteReady = 0;
        return key;
    }
    return 0;
}

void TIM4_IRQHandler(void)
{
    // 溢出中断 (处理超时/连发码)
    if(TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) {
        if(s_RmtState != RMT_IDLE) {
            s_RmtState = RMT_IDLE; // 超时复位
            s_RmtBitCnt = 0;
        }
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    }

    if(TIM_GetITStatus(TIM4, TIM_IT_CC4) != RESET) {
        uint16_t duration = TIM_GetCapture4(TIM4); 
        TIM_SetCounter(TIM4, 0); // 清零计数器，准备测下一段

        switch(s_RmtState) {
            case RMT_IDLE:
                s_RmtState = RMT_HEADER;
                break;

            case RMT_HEADER:0
                if(duration > 12000 && duration < 15000) {
                    s_RmtData = 0;
                    s_RmtBitCnt = 0;
                    s_RmtState = RMT_DATA;
                } 
                // 连发码 (9ms + 2.5ms = 11.5ms)，暂不支持连发，或者复位
                else if(duration > 10000 && duration < 12000) {
                    s_RmtState = RMT_IDLE; 
                }
                else {
                    s_RmtState = RMT_IDLE; // 噪声
                }
                break;

            case RMT_DATA:
                
                if(duration > 400 && duration < 2500) {
                    s_RmtData >>= 1; // 低位在前
                    if(duration > 1600) {
                        s_RmtData |= 0x80000000;
                    }
                    s_RmtBitCnt++;
                    
                    if(s_RmtBitCnt >= 32) {
                        // 接收完成，解析数据
                        // NEC格式: [地址8bit][~地址8bit][数据8bit][~数据8bit]
                        uint8_t d0 = (s_RmtData >> 24) & 0xFF;
                        uint8_t d1 = (s_RmtData >> 16) & 0xFF;
                        uint8_t d2 = (s_RmtData >> 8) & 0xFF;
                        uint8_t d3 = (s_RmtData >> 0) & 0xFF;
                        
                        // 校验：数据码 + 反码 = 0xFF
                        if((d0 + d1) == 0xFF) {
                            g_RemoteKey = d1;
                            g_RemoteReady = 1;
                        }
                        s_RmtState = RMT_IDLE;
                    }
                } else {
                    s_RmtState = RMT_IDLE; // 错误脉宽
                }
                break;
        }
        TIM_ClearITPendingBit(TIM4, TIM_IT_CC4);
    }
}