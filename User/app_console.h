/* 串口命令行对外接口（不重烧程序就能改参数）。 */
#ifndef _app_console_h_
#define _app_console_h_

#include "zf_common_headfile.h"

void app_console_init(void);
void app_console_task(void);            /* 主循环调用, 非阻塞解析串口命令 */

#endif
