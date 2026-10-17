/* UI 绘制原语接口，供菜单页与调试页共用。 */
#ifndef _app_ui_h_
#define _app_ui_h_

#include "zf_common_headfile.h"

/* ---- 列表绘制原语（菜单树共用）---- */
#define APP_UI_ROW_TOP          (16)
#define APP_UI_ROW_HEIGHT       (16)
#define APP_UI_ROW_MAX          (8)
#define APP_UI_HINT_Y           (144)

/* ---- 底部提示语：竖屏 8x16 字体一行只能放 16 个字符，超过会被裁掉 ---- */
#define APP_UI_HINT_LIST        "U/D move M ok K2"      /* 列表页(1/2 级) 16 字符 */
#define APP_UI_HINT_EDIT        "L/R chg M ok K2"       /* 编辑页(3 级)  15 字符 */

void  app_ui_init(void);
void  app_ui_task(void);

void  app_ui_list_begin (const char *title);                    /* 清屏 + 标题栏 + 提示栏 */
void  app_ui_list_title (const char *title);                    /* 只重画标题栏 */
void  app_ui_list_row   (uint8 row, const char *text, uint8 highlight);
void  app_ui_list_clear_rows(uint8 count);                      /* 把多余的行擦干净 */
void  app_ui_list_hint  (const char *hint);                     /* 底部提示条 */
void  app_ui_msg        (const char *msg);                      /* 临时消息(红底), 1.5s 恢复 */

void  app_ui_text       (uint16 x, uint16 y, const char *text, const uint16 pen, const uint16 bg);
void  app_ui_page_keydebug(void);                               /* 按键调试页(原 step9 的) */

#endif
