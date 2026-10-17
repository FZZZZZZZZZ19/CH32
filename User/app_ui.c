/* ======================== 【为什么要有这个文件】 ========================  * 三级菜单、按键调试页、参数编辑页，画法几乎一样：一行标题 + 若干行 + 底部提示。  * 把这些画一行 / 擦一行 / 高亮一行抽出来之后，菜单代码里只剩状态机逻辑，  * 不再混着 tft180_show_string(x, y, ...) 的坐标准算。  *  * 为什么只重画变化的行：tft180_show_string() 会同时写前景和背景（不透明），  * 所以擦一行 = 用背景色重写一行；整屏重画在 10Hz 下会明显闪烁。  * 为什么消息条会自动恢复：提示（已保存）不能永久占屏，否则菜单就看不见了。  * ====================================================================== */ 
 #include "app_config.h"
#include "app_ui.h"
#include "app_key.h"
#include "app_tick.h"

/*-------------------------------------------------------------------------------------------------------------------
 * 列表绘制原语（步骤 10 §10.4 "app_ui.c 里关键实现" 原文）
 *-----------------------------------------------------------------------------------------------------------------*/
void app_ui_list_begin(const char *title)
{
    tft180_set_color(RGB565_BLACK, RGB565_WHITE);
    tft180_clear();
    app_ui_list_title(title);
}

void app_ui_list_title(const char *title)
{
    char line[20];

    tft180_set_color(RGB565_WHITE, RGB565_BLUE);
    sprintf(line, "%-16s", title);
    tft180_show_string(0, 0, line);
    tft180_set_color(RGB565_BLACK, RGB565_WHITE);
}

void app_ui_list_row(uint8 row, const char *text, uint8 highlight)
{
    uint16 y = (uint16)(APP_UI_ROW_TOP + row * APP_UI_ROW_HEIGHT);

    tft180_set_color(highlight ? RGB565_WHITE : RGB565_BLACK,
                     highlight ? RGB565_BLUE  : RGB565_WHITE);
    tft180_show_string(0, y, text);
    tft180_set_color(RGB565_BLACK, RGB565_WHITE);
}

void app_ui_list_clear_rows(uint8 count)
{
    uint8 i;
    for(i = count; i < APP_UI_ROW_MAX; i ++)
    {
        app_ui_list_row(i, "                ", 0);       /* 16 个空格 */
    }
}

/*-------------------------------------------------------------------------------------------------------------------
 * 底部提示/消息行（步骤 9 的 app_ui_draw_hint / app_ui_message 合并成
 * app_ui_list_hint / app_ui_msg —— 步骤 10 只声明、未给实现）
 *-----------------------------------------------------------------------------------------------------------------*/
static char   ui_msg[17]     = {0};         /* 临时消息(最多 16 字符) */
static uint32 ui_msg_tick    = 0;

void app_ui_list_hint(const char *hint)
{
    char line[20];

    tft180_set_color(RGB565_WHITE, (ui_msg[0]) ? RGB565_RED : RGB565_GRAY);
    sprintf(line, "%-16s", hint);
    tft180_show_string(0, APP_UI_HINT_Y, line);
    tft180_set_color(RGB565_BLACK, RGB565_WHITE);
}

void app_ui_msg(const char *msg)
{
    uint8 i;

    for(i = 0; (i < 16) && (msg[i]); i ++)
    {
        ui_msg[i] = msg[i];
    }
    ui_msg[i]   = '\0';
    ui_msg_tick = app_tick_get();
    app_ui_list_hint(ui_msg);
}

/*-------------------------------------------------------------------------------------------------------------------
 * 任意位置写一行文本（步骤 10 声明了 app_ui_text(), 但没给实现 —— 这里按声明补上）
 *-----------------------------------------------------------------------------------------------------------------*/
void app_ui_text(uint16 x, uint16 y, const char *text, const uint16 pen, const uint16 bg)
{
    tft180_set_color(pen, bg);
    tft180_show_string(x, y, text);
    tft180_set_color(RGB565_BLACK, RGB565_WHITE);
}

/*-------------------------------------------------------------------------------------------------------------------
 * 按键调试页（步骤 9 的 app_ui_draw_keydebug, 步骤 10 改名为 app_ui_page_keydebug）
 *-----------------------------------------------------------------------------------------------------------------*/
void app_ui_page_keydebug(void)
{
    char    line[20];
    uint8   code = app_key_get_raw_code();
    app_key_enum key = app_key_get_pressed();

    tft180_set_color(RGB565_BLACK, RGB565_WHITE);
    tft180_clear();

    tft180_set_color(RGB565_WHITE, RGB565_BLUE);
    tft180_show_string(0, 0, "KEY DEBUG       ");
    tft180_set_color(RGB565_BLACK, RGB565_WHITE);

    sprintf(line, "A2 B14 :   %d    ", (code >> 2) & 1);   tft180_show_string(0,  32, line);
    sprintf(line, "A1 B12 :   %d    ", (code >> 1) & 1);   tft180_show_string(0,  48, line);
    sprintf(line, "A0 D8  :   %d    ", (code >> 0) & 1);   tft180_show_string(0,  64, line);
    sprintf(line, "code   :   %d    ", code);              tft180_show_string(0,  88, line);
    sprintf(line, "key    : %s    ", app_key_name(key));   tft180_show_string(0, 112, line);
    tft180_set_color(RGB565_WHITE, RGB565_RED);
    tft180_show_string(0, 144, "press any to back");
    tft180_set_color(RGB565_BLACK, RGB565_WHITE);
}

/*-------------------------------------------------------------------------------------------------------------------
 * 初始化（步骤 9 的 app_ui_init, 去掉已删除的 ui_items 部分）
 * 备注信息     tft180_set_dir() 必须在 tft180_init() 之前调:
 *              源码里 set_dir() 只改 tft180_display_dir/x_max/y_max 三个变量, 真正写
 *              ST7735 MADCTL 的是 tft180_init() 里的 tft180_set_dir(tft180_display_dir)
 *              —— 顺序反了就只改了坐标范围、屏幕扫描方向不会变。
 *-----------------------------------------------------------------------------------------------------------------*/
void app_ui_init(void)
{
    tft180_set_dir(TFT180_PORTAIT);         /* 128 宽 x 160 高 */
    tft180_init();
    tft180_set_font(TFT180_8X16_FONT);
    tft180_set_color(RGB565_BLACK, RGB565_WHITE);
    tft180_clear();

    printf("[UI ] tft180 ready, dir=PORTAIT 128x160\r\n");
}

/*-------------------------------------------------------------------------------------------------------------------
 * 刷新（步骤 9 的 app_ui_task, 去掉已删除的 ui_items 部分）
 * 备注信息     步骤 10 §10.6 的 app_task() 没有调用本函数 —— 1.5s 消息恢复不会发生。
 *-----------------------------------------------------------------------------------------------------------------*/
void app_ui_task(void)
{
    /* ---------- 提示消息超时 ---------- */
    if((ui_msg[0]) && ((app_tick_get() - ui_msg_tick) > 1500))
    {
        ui_msg[0] = '\0';
        app_ui_list_hint("U/D move  M ok  K2 back");
    }
}
