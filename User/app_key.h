/* 74HC148 按键接口。自定义驱动，不能用逐飞的 zf_device_key。 */
#ifndef _app_key_h_
#define _app_key_h_

#include "zf_common_headfile.h"

typedef enum
{
    APP_KEY_NONE = 0,
    APP_KEY_KEY1,               /* SW14        code 3 */
    APP_KEY_KEY2,               /* SW15        code 0 */
    APP_KEY_UP,                 /* 五向 上     code 4 */
    APP_KEY_DOWN,               /* 五向 下     code 5 */
    APP_KEY_LEFT,               /* 五向 左     code 6 */
    APP_KEY_RIGHT,              /* 五向 右     code 2 */
    APP_KEY_MID,                /* 五向 中     code 1 */
    APP_KEY_NUMBER
} app_key_enum;

typedef enum
{
    APP_KEY_EVENT_NONE = 0,
    APP_KEY_EVENT_PRESS,        /* 刚按下（已消抖） */
    APP_KEY_EVENT_LONG,         /* 按住超过 APP_KEY_LONG_MS */
    APP_KEY_EVENT_REPEAT        /* 长按之后的周期性连发 */
} app_key_event_enum;

void               app_key_init(void);
void               app_key_task(void);                              /* 10ms 调一次 */
app_key_enum       app_key_get_pressed(void);                       /* 当前按住的键 */
app_key_event_enum app_key_get_event(app_key_enum *key);            /* 取事件, 取完即清 */
uint8              app_key_get_raw_code(void);                      /* 原始 3bit, 调试页用 */
const char *       app_key_name(app_key_enum key);                  /* 调试页用的键名 */

#endif
