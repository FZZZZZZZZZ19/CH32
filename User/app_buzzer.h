/* 蜂鸣器对外接口。第 6 步起用非阻塞版 start()+task()。 */
#ifndef _app_buzzer_h_
#define _app_buzzer_h_

#include "zf_common_headfile.h"

void app_buzzer_init(void);
void app_buzzer_on(uint32 freq);                    /* 开始发声（不返回，立即响）     */
void app_buzzer_off(void);                          /* 停止发声                       */
void app_buzzer_beep(uint32 freq, uint32 ms);       /* 【阻塞版】仅调试用，别在正式代码里用 */

void app_buzzer_start(uint32 freq, uint32 ms);      /* 响 ms 毫秒后自动停（非阻塞）   */
void app_buzzer_task(void);                         /* 放在主循环里调用              */

#endif
