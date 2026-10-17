/* ======================== 【为什么要有这个文件】 ========================
 * 三级菜单、按键调试页、参数编辑页，画法几乎一样：一行标题 + 若干行 + 底部提示。
 * 把这些画一行 / 擦一行 / 高亮一行抽出来之后，菜单代码里只剩状态机逻辑，
 * 不再混着 tft180_show_string(x, y, ...) 的坐标准算。
 *
 * 为什么只重画变化的行：tft180_show_string() 会同时写前景和背景（不透明），
 * 所以擦一行 = 用背景色重写一行；整屏重画在 10Hz 下会明显闪烁。
 * 为什么消息条会自动恢复：提示（已保存）不能永久占屏，否则菜单就看不见了。
 *
 * ★ 为什么所有字符串都要先过 app_ui_show_clip()：
 *   8x16 字体在 128 宽的竖屏上只有 16 列。逐飞驱动里 tft180_show_char() 开头是
 *       zf_assert(x < tft180_x_max);
 *       zf_assert(y < tft180_y_max);
 *   只要有一行文本超过 16 个字符，第 17 个字符的 x 就是 128，直接触发断言，
 *   现象就是屏幕定在 "Assert erro file:zf_device/zf_device_tft180.c line:409"
 *   （这一行是 .c 文件里的：line 409）。
 *   所以这里做了唯一一个写字符串的入口：超长截断、不足补空格、并且保证 x<128。
 *   以后再加菜单项 / 提示语，只要走这个函数就永远不会再把屏幕打挂。
 * ====================================================================== */
#include "app_config.h"
#include "app_ui.h"
#include "app_key.h"
#include "app_tick.h"

/* 屏幕几何：竖屏 PORTAIT 时 tft180_x_max=128、tft180_y_max=160（见 zf_device_tft180.c） */
#define APP_UI_WIDTH        (128)
#define APP_UI_CHAR_W       (8)                         /* TFT180_8X16_FONT 的字符宽 */
#define APP_UI_COLS         (APP_UI_WIDTH / APP_UI_CHAR_W)      /* = 16 列 */

/*-------------------------------------------------------------------------------------------------------------------
 * 唯一的字符串输出入口：裁剪到 max_cols 列，不足补空格（保证 x 永远 < 128）
 * 参数说明     x/y         起始坐标
 * 参数说明     text        任意长度字符串（可以为 NULL 之外的合法指针）
 * 参数说明     max_cols    这一行最多画几列
 *-----------------------------------------------------------------------------------------------------------------*/
static void app_ui_show_clip(uint16 x, uint16 y, const char *text, uint8 max_cols)
{
    char  line[APP_UI_COLS + 1];
    uint8 i;

    if((0 == max_cols) || (x >= APP_UI_WIDTH))
    {
        return;
    }
    if(max_cols > APP_UI_COLS)
    {
        max_cols = APP_UI_COLS;
    }
    for(i = 0; i < max_cols; i ++)
    {
        line[i] = (text[i]) ? text[i] : ' ';            /* 到字符串结尾就补空格 */
    }
    line[max_cols] = '\0';

    tft180_show_string(x, y, line);
}

/*-------------------------------------------------------------------------------------------------------------------
 * 列表绘制原语
 *-----------------------------------------------------------------------------------------------------------------*/
void app_ui_list_begin(const char *title)
{
    tft180_set_color(RGB565_BLACK, RGB565_WHITE);
    tft180_clear();
    app_ui_list_title(title);
}

void app_ui_list_title(const char *title)
{
    tft180_set_color(RGB565_WHITE, RGB565_BLUE);
    app_ui_show_clip(0, 0, title, APP_UI_COLS);
    tft180_set_color(RGB565_BLACK, RGB565_WHITE);
}

void app_ui_list_row(uint8 row, const char *text, uint8 highlight)
{
    uint16 y = (uint16)(APP_UI_ROW_TOP + row * APP_UI_ROW_HEIGHT);

    tft180_set_color(highlight ? RGB565_WHITE : RGB565_BLACK,
                     highlight ? RGB565_BLUE  : RGB565_WHITE);
    app_ui_show_clip(0, y, text, APP_UI_COLS);
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
 * 底部提示/消息行（步骤 9 的 app_ui_draw_hint / app_ui_message 合并而来）
 *-----------------------------------------------------------------------------------------------------------------*/
static char   ui_msg[17]     = {0};         /* 临时消息(最多 16 字符, 第 17 字节放 '\0') */
static uint32 ui_msg_tick    = 0;

void app_ui_list_hint(const char *hint)
{
    tft180_set_color(RGB565_WHITE, (ui_msg[0]) ? RGB565_RED : RGB565_GRAY);
    app_ui_show_clip(0, APP_UI_HINT_Y, hint, APP_UI_COLS);
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
 * 任意位置写一行文本（自动裁掉右侧装不下的部分）
 *-----------------------------------------------------------------------------------------------------------------*/
void app_ui_text(uint16 x, uint16 y, const char *text, const uint16 pen, const uint16 bg)
{
    tft180_set_color(pen, bg);
    app_ui_show_clip(x, y, text, (uint8)((APP_UI_WIDTH - x) / APP_UI_CHAR_W));
    tft180_set_color(RGB565_BLACK, RGB565_WHITE);
}

/*-------------------------------------------------------------------------------------------------------------------
 * 按键调试页（步骤 9 的 app_ui_draw_keydebug, 步骤 10 改名为 app_ui_page_keydebug）
 *-----------------------------------------------------------------------------------------------------------------*/
void app_ui_page_keydebug(void)
{
    char    line[24];
    uint8   code = app_key_get_raw_code();
    app_key_enum key = app_key_get_pressed();

    tft180_set_color(RGB565_BLACK, RGB565_WHITE);
    tft180_clear();

    tft180_set_color(RGB565_WHITE, RGB565_BLUE);
    app_ui_show_clip(0, 0, "KEY DEBUG", APP_UI_COLS);
    tft180_set_color(RGB565_BLACK, RGB565_WHITE);

    sprintf(line, "A2 B14 :   %d", (code >> 2) & 1);   app_ui_show_clip(0,  32, line, APP_UI_COLS);
    sprintf(line, "A1 B12 :   %d", (code >> 1) & 1);   app_ui_show_clip(0,  48, line, APP_UI_COLS);
    sprintf(line, "A0 D8  :   %d", (code >> 0) & 1);   app_ui_show_clip(0,  64, line, APP_UI_COLS);
    sprintf(line, "code   :   %d", code);              app_ui_show_clip(0,  88, line, APP_UI_COLS);
    sprintf(line, "key    : %s", app_key_name(key));   app_ui_show_clip(0, 112, line, APP_UI_COLS);

    tft180_set_color(RGB565_WHITE, RGB565_RED);
    app_ui_show_clip(0, APP_UI_HINT_Y, "any key = back", APP_UI_COLS);
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

    printf("[UI ] tft180 ready, dir=PORTAIT 128x160, %d cols\r\n", APP_UI_COLS);
}

/*-------------------------------------------------------------------------------------------------------------------
 * 刷新：提示消息超时后恢复成常规提示条
 *-----------------------------------------------------------------------------------------------------------------*/
void app_ui_task(void)
{
    /* ---------- 提示消息超时 ---------- */
    if((ui_msg[0]) && ((app_tick_get() - ui_msg_tick) > 1500))
    {
        ui_msg[0] = '\0';
        app_ui_list_hint(APP_UI_HINT_LIST);     /* "U/D move M ok K2" */
    }
}
