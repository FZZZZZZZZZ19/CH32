/* 屏幕对外接口（含自绘 rect/bar，因为逐飞库没有画矩形的 API）。 */
#ifndef _app_display_h_
#define _app_display_h_

#include "zf_common_headfile.h"

void app_display_init(void);                                    /* 初始化 + 画静态页 */
void app_display_task(void);                                    /* 主循环调用, 10Hz 刷新 */

/* 通用绘图小工具（本库没有画矩形的接口，所以自己用 4 条线拼） */
void app_display_rect(uint16 x1, uint16 y1, uint16 x2, uint16 y2, uint16 color);
void app_display_bar(uint16 x, uint16 y, uint16 width, uint16 height,
                     uint32 value, uint32 value_max, uint16 color);

/* 给步骤 9 的菜单预留：显示"当前按键" */
void app_display_bind_key(const char *key_name);

#endif
