/* 为什么单独一个头文件：中断入口不在这里 —— 所有 IRQHandler 集中在 User/zf_isr.c，  * 便于一眼看全优先级与冲突；这里只暴露读时间/判断到点/中断里调用三件事。 */ 
 /********************************************************************************
* File Name          : app_tick.h
* Description        : 1ms 系统节拍 + 非阻塞定时调度
*                      中断函数写在 User/zf_isr.c 的 TIM6_IRQHandler 里
********************************************************************************/
#ifndef _app_tick_h_
#define _app_tick_h_

#include "zf_common_headfile.h"

extern volatile uint32 g_app_tick_ms;           /* 上电以来的毫秒数，只增不减       */

void   app_tick_init(void);                     /* 启动 1ms 定时器中断              */
uint32 app_tick_get(void);                      /* 读当前毫秒数                     */
uint8  app_tick_reached(uint32 *last_ms, uint32 period_ms);   /* 周期到了返回 1      */
void   app_tick_irq(void);                      /* 中断服务：只能由 zf_isr.c 调用    */

#endif
