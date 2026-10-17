/* ======================== 【为什么要有这个文件】 ========================  * main.c 里只该有三行：配时钟、开调试串口、调 app_init()/app_task()。  * 有了这个头文件，main.c 不需要 include 任何 app_xxx.h；  * 以后加模块、删模块，main.c 一个字都不用改 —— 这就是分层最直接的收益。  * ====================================================================== */ 
 #ifndef _app_h_
#define _app_h_

#include "zf_common_headfile.h"

void  app_init(void);                       /* 上电初始化：依次初始化所有模块 */
void  app_task(void);                       /* 主循环调度：一圈必须很快，不能阻塞 */

void  app_breath_set_step(int32 step);      /* 改呼吸灯步进（菜单里用） */
int32 app_breath_get_duty(void);            /* 读当前占空比（屏幕显示用） */
uint8 app_counter_get(void);                /* 读二进制计数器的低 3 位 */

#endif
