/* ======================== 【为什么要有这个文件】 ========================  * 屏幕是唯一有状态的外设：静态页只该画一次，变化的值才刷新。若各模块  * 各自调 tft180_show_string()，屏幕会闪、也会互相覆盖。这里统一收口：  * app_display_init() 画静态页，app_display_task() 按 10Hz 只刷变化的值。  *  * 为什么要自己写 rect/bar：逐飞 tft180 驱动只有点、线、字符，没有画矩形的  * 接口（内部的 set_region 是 static 的没暴露），所以用 4 条线拼一个矩形。  * 为什么刷新要节流到 10Hz：刷一次屏要几毫秒，跑在主循环里会拖慢按键响应。  * ====================================================================== */ 
 #include "app_config.h"
#include "app_display.h"
#include "app_tick.h"
#include "app_led.h"
#include "app_buzzer.h"
#include "app.h"

/* 屏幕坐标（竖屏）*/
#define DISP_W                      (128)
#define DISP_H                      (160)

static uint32 disp_last      = 0;           /* 刷新节流             */
static uint32 disp_last_sec  = 0xFFFFFFFF;  /* 脏值记录：秒数        */
static uint32 disp_last_ms   = 0xFFFFFFFF;  /* 脏值记录：毫秒        */
static int32  disp_last_duty = -1;          /* 脏值记录：占空比      */
static uint8  disp_last_cnt  = 0xFF;        /* 脏值记录：计数        */
static const char *disp_key_name = "NONE";  /* 当前按键名（步骤 9）  */

extern volatile uint32 g_app_tick_ms;

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     用 4 条线拼一个矩形
 * 备注信息     逐飞 tft180 库没有提供画矩形接口, 所以这里自己拼(答辩时可作为"库不够用就自己补"的例子)
 *-----------------------------------------------------------------------------------------------------------------*/
void app_display_rect(uint16 x1, uint16 y1, uint16 x2, uint16 y2, uint16 color)
{
    tft180_draw_line(x1, y1, x2, y1, color);        /* 上 */
    tft180_draw_line(x1, y2, x2, y2, color);        /* 下 */
    tft180_draw_line(x1, y1, x1, y2, color);        /* 左 */
    tft180_draw_line(x2, y1, x2, y2, color);        /* 右 */
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     画进度条
 * 参数说明     value/value_max   0 ~ value_max 映射到 0 ~ width
 * 备注信息     先整条用背景色刷掉, 再画填充部分 —— 典型的"局部重绘"
 *-----------------------------------------------------------------------------------------------------------------*/
void app_display_bar(uint16 x, uint16 y, uint16 width, uint16 height,
                     uint32 value, uint32 value_max, uint16 color)
{
    uint16 i;
    uint16 fill;

    if(0 == value_max)
    {
        return;
    }

    if(value > value_max)
    {
        value = value_max;
    }

    fill = (uint16)((uint32)width * value / value_max);

    for(i = 0; i < height; i ++)
    {
        tft180_draw_line(x, (uint16)(y + i), (uint16)(x + width - 1), (uint16)(y + i), APP_DISPLAY_BG);   /* 清空 */
        if(fill > 0)
        {
            tft180_draw_line(x, (uint16)(y + i), (uint16)(x + fill - 1), (uint16)(y + i), color);          /* 填充 */
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     画"只画一次"的静态页面
 * 备注信息     标题、分隔线、标签、脚注 —— 这些内容全程不变, 每帧重画纯属浪费且会闪
 *-----------------------------------------------------------------------------------------------------------------*/
static void app_display_static_page(void)
{
    tft180_full(APP_DISPLAY_BG);                        /* 整屏刷背景色 */

    tft180_set_color(RGB565_BLUE, APP_DISPLAY_BG);
    tft180_show_string(0, 0, "CH32V307  TFT180");

    tft180_draw_line(0, 17, DISP_W - 1, 17, RGB565_BLUE);   /* 分隔线 */

    tft180_set_color(APP_DISPLAY_PEN, APP_DISPLAY_BG);
    tft180_show_string(0, 32, "Tick:");
    tft180_show_string(0, 48, "Duty:");
    tft180_show_string(0, 64, "Cnt :");
    tft180_show_string(0, 96, "Time:");                     /* 秒数 */
    tft180_show_string(0, 112, "Key :");
    tft180_show_string(0, 136, "SPI2 B13/B15");             /* 脚注：静态 */
    tft180_show_string(0, 148, "DC D7  CS D4 ");

    app_display_rect(0, 80, DISP_W - 1, 89, RGB565_GRAY);   /* 进度条外框 */
}

void app_display_init(void)
{
    tft180_set_dir(APP_DISPLAY_DIR);        /* ★ 必须在 init 之前 */
    tft180_init();
    tft180_set_font(APP_DISPLAY_FONT);
    tft180_set_color(APP_DISPLAY_PEN, APP_DISPLAY_BG);

    app_display_static_page();

    printf("[APP] tft180 init done, dir=%d, %dx%d\r\n", APP_DISPLAY_DIR, DISP_W, DISP_H);
}

void app_display_bind_key(const char *key_name)
{
    disp_key_name = key_name;
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     屏幕刷新任务（10 Hz, 只重画变化的值）
 *-----------------------------------------------------------------------------------------------------------------*/
void app_display_task(void)
{
    uint32 now_ms  = g_app_tick_ms;
    uint32 now_sec = now_ms / 1000;
    int32  duty    = app_breath_get_duty();         /* 见 7.4, 从 app.c 暴露出来 */
    uint8  cnt     = app_counter_get();

    if(0 == app_tick_reached(&disp_last, APP_DISPLAY_REFRESH_MS))
    {
        return;                                     /* 还没到刷新周期 */
    }

    if(now_ms != disp_last_ms)                      /* 脏值判断: 只在变了才画 */
    {
        tft180_show_uint(48, 32, now_ms, 10);
        disp_last_ms = now_ms;
    }

    if(duty != disp_last_duty)
    {
        tft180_show_int(48, 48, duty, 5);
        app_display_bar(1, 81, DISP_W - 2, 8, (uint32)duty, PWM_DUTY_MAX, RGB565_GREEN);
        disp_last_duty = duty;
    }

    if(cnt != disp_last_cnt)
    {
        tft180_show_int(48, 64, cnt, 3);
        disp_last_cnt = cnt;
    }

    if(now_sec != disp_last_sec)
    {
        tft180_show_uint(48, 96, now_sec, 6);
        disp_last_sec = now_sec;
    }

    tft180_show_string(48, 112, disp_key_name);     /* 按键名（步骤 9 才有内容） */
}
