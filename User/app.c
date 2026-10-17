/********************************************************************************
* File Name          : app.c
* Description        : 应用层总调度 —— 第 9 步版（+ 按键读取 + 屏幕 UI 交互）
*
* 【这一步在干什么】
*   板上的 8 个按键没有直接接到 8 个 IO，而是先过一个 74HC148 优先编码器，
*   变成 3 根线（PD8 / PB12 / PB14）送进单片机。所以"读按键"要先解码、再消抖。
*   app_key.c 负责把 3 根线翻译成"哪个键、单击还是长按"；
*   app_ui.c  负责把结果画到屏幕上，并且只重画变化的行（不闪屏）。
*
* 【为什么按键扫描放在主循环、不放中断里】
*   真按键按下时触点会抖动几毫秒 —— 必须"隔一小会儿再看一眼"，这是时间上的事，
*   中断里不适合做（会拉长中断时间）。而且 74HC148 是共用的 3 根线，
*   中断只适合"提示有变化"，判断和消抖仍然要放到 10ms 的任务里。
********************************************************************************/
#include "app_config.h"
#include "app.h"
#include "app_led.h"
#include "app_buzzer.h"
#include "app_tick.h"
#include "app_display.h"
#include "app_param.h"
#include "app_console.h"
#include "app_key.h"                /* 第 9 步新增：74HC148 解码 + 消抖 */
#include "app_ui.h"                 /* 第 9 步新增：屏幕 UI 交互       */

/* ================================ 呼吸灯 ================================ */
static int32 breath_duty = 0;
static int32 breath_dir  = 1;
static int32 breath_step = APP_BREATH_STEP;

static void app_breath_init(void)
{
    pwm_init(APP_BREATH_PWM, APP_BUZZER_FREQ_DEFAULT, 0);
}

void app_breath_set_step(int32 step)
{
    if(step < 1)    { step = 1;    }
    if(step > 1000) { step = 1000; }
    breath_step = step;
}

static void app_breath_task(void)
{
    breath_duty += breath_dir * breath_step;

    if(breath_duty >= PWM_DUTY_MAX) { breath_duty = PWM_DUTY_MAX; breath_dir = -1; }
    else if(breath_duty <= 0)       { breath_duty = 0;            breath_dir =  1; }

    pwm_set_duty(APP_BREATH_PWM, (uint32)breath_duty);
}

int32 app_breath_get_duty(void) { return breath_duty; }

/* ============================== 二进制计数 ============================== */
static uint32 counter = 0;

uint8 app_counter_get(void) { return (uint8)(counter & 0x07); }

/* =========================== 非阻塞定时的时间戳 =========================== */
static uint32 t_count = 0;
static uint32 t_beep  = 0;
static uint32 t_print = 0;
static uint32 t_run   = 0;
static uint32 t_key   = 0;          /* 按键扫描周期（第 9 步新增）            */

/* ============================== 上电初始化 ==============================
 * 【为什么按键初始化放在屏幕之后】
 *   没有严格的先后依赖，但屏幕初始化最慢（约 250ms），
 *   把按键配在前面会让"第一次按键"在屏幕还没亮的这段时间里没有反应；
 *   顺序统一成"快的在前、慢的在最后"，出问题时也容易定位。
 * --------------------------------------------------------------------*/
void app_init(void)
{
    app_led_init();
    app_buzzer_init();
    app_breath_init();

    app_param_load();
    app_param_apply();
    app_breath_set_step((int32)g_param.breath_step);

    app_tick_init();

    /* 顺序有讲究：app_ui_init() 里会 tft180_clear() 清屏，
       所以它必须排在 app_display_init()（画静态页）之前，否则刚画好的页会被清掉。 */
    app_key_init();                     /* ★ 第 9 步：PD8/PB12/PB14 配成上拉输入 */
    app_ui_init();                      /* ★ 第 9 步：屏幕初始化（清屏 + 字体） */
    app_display_init();                 /* 画静态页（Tick/Cnt/Duty/Time/Key）   */

    app_console_init();
    app_param_show();

    printf("[APP] step9: + key(74hc148) + ui\r\n");
}

/* ================================ 主循环 ================================ */
void app_task(void)
{
    app_led_task();
    app_buzzer_task();
    app_breath_task();

    /* --- 10ms：按键扫描（消抖、判长按都在里面）--- */
    if(app_tick_reached(&t_key, 10))
    {
        app_key_task();
        /* 屏幕 "Key :" 那一行跟着当前按住的键实时变（app_display 本来就留了这个接口） */
        app_display_bind_key(app_key_name(app_key_get_pressed()));
    }

    /* --- 按键事件：串口打印 + KEY1 响一声（证明消抖/长按状态机真的在跑）--- */
    {
        app_key_enum       ev_key;
        app_key_event_enum ev = app_key_get_event(&ev_key);

        if(APP_KEY_EVENT_NONE != ev)
        {
            printf("[KEY] %s %s (code=%d)\r\n", app_key_name(ev_key),
                   (APP_KEY_EVENT_PRESS  == ev) ? "PRESS"  :
                   (APP_KEY_EVENT_LONG   == ev) ? "LONG"   : "REPEAT",
                   app_key_get_raw_code());

            if(APP_KEY_KEY1 == ev_key)                  /* SW14：按一下嗡一声 */
            {
                app_buzzer_start(g_param.buzzer_freq, 50);
            }
        }
    }

    if(app_tick_reached(&t_count, 100))
    {
        counter ++;
        if(0 == app_led_is_busy())
        {
            app_led_show_bin(app_counter_get());
        }
    }

    if(app_tick_reached(&t_beep, 1000))
    {
        app_buzzer_start(g_param.buzzer_freq, 50);
    }

    if(app_tick_reached(&t_run, 1000))
    {
        g_param.run_seconds ++;
    }

    if(app_tick_reached(&t_print, 500))
    {
        printf("[APP] tick=%u s, cnt=%u\r\n",
               (unsigned int)(app_tick_get() / 1000), (unsigned int)app_counter_get());
    }

    /* --- 界面与命令行（都是"有活就干、没活立刻返回"）--- */
    app_ui_task();                      /* ★ 第 9 步：刷新页面 / 处理临时消息 */
    app_display_task();
    app_console_task();
}
