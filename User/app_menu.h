/* 三级菜单对外接口。 */
#ifndef _app_menu_h_
#define _app_menu_h_

#include "zf_common_headfile.h"

void app_menu_init(void);
void app_menu_task(void);           /* 主循环调用: 消费按键事件 + 重画 */

#endif
