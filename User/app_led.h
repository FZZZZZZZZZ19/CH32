/* 4 路 LED 的对外接口。注意 index 0（PA11）已让给 PWM 呼吸灯。 */
#ifndef _app_led_h_
#define _app_led_h_

#include "zf_common_headfile.h"

void  app_led_init(void);                   /* 初始化 4 路 LED（默认全灭）      */
void  app_led_set(uint8 index, uint8 on);   /* index: 0~3, on: 0=灭 1=亮        */
void  app_led_toggle(uint8 index);
void  app_led_all(uint8 on);
uint8 app_led_get(uint8 index);             /* 读回软件记录的状态               */

void  app_led_blink_set(uint8 index, uint8 enable);     /* 让某一路加入/退出闪烁组   */
void  app_led_blink_interval(uint32 ms);                /* 统一设置闪烁周期          */
void  app_led_task(void);                               /* 主循环调用                */
void  app_led_show_bin(uint8 value);                    /* 用 3 路 LED 显示 0~7       */
void  app_led_chase_toggle(void);                        /* 流水灯 开/关（只用 A12/A15/D1）*/
uint8 app_led_is_busy(void);                            /* 有闪烁/流水在占用 LED 时返回 1 */

#endif
