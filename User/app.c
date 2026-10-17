/********************************************************************************
* File Name          : app.c
* Description        : 应用层总调度 —— 第 7 步版（+ 1.8 寸 TFT 屏幕）
*
* 【这一步在干什么】
*   把屏幕接进来：app_display_init() 里做完屏幕初始化并画一次静态页面，
*   之后主循环每圈调用 app_display_task()，模块内部自己按 10Hz 刷新变化的值。
*
* 【为什么屏幕要单独一个模块文件】
*   屏幕的初始化很慢（逐飞驱动里有 10ms + 120ms + 120ms 的等待，约 250ms），
*   而且它是唯一一个"有状态显示"的外设（画静态页 / 局部刷新 / 脏值比较）。
*   把这些关在 app_display.c 里，别的模块不用知道屏幕怎么画。
********************************************************************************/
#include "app_config.h"
#include "app.h"
#include "app_led.h"
#include "app_buzzer.h"
#include "app_tick.h"
#include "app_display.h"            /* 第 7 步新增 */

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

/* ============================== 上电初始化 ==============================
 * 【为什么顺序是这样】
 *   1) LED / 蜂鸣器 / 呼吸灯：纯 GPIO + PWM，最快，先起来；
 *   2) 1ms 节拍：让时间基准开始走；
 *   3) 屏幕：最慢（约 250ms 的等待），放最后，不影响前面的东西先跑起来。
 * --------------------------------------------------------------------*/
void app_init(void)
{
    app_led_init();
    app_buzzer_init();
    app_breath_init();
    app_tick_init();

    app_display_init();                 /* ★ 第 7 步新增：初始化屏幕 + 画静态页 */

    printf("[APP] step7: + tft180 ready\r\n");
}

/* ================================ 主循环 ================================
 * app_display_task() 内部用 app_tick_reached(&disp_last, 100) 自己按 10Hz 节流，
 * 所以这里可以每圈调用，不用再套一层 if。
 * --------------------------------------------------------------------*/
void app_task(void)
{
    app_led_task();
    app_buzzer_task();
    app_breath_task();

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
        app_buzzer_start(APP_BUZZER_FREQ_DEFAULT, 50);
    }

    if(app_tick_reached(&t_print, 500))
    {
        printf("[APP] tick=%u s, cnt=%u, duty=%d\r\n",
               (unsigned int)(app_tick_get() / 1000),
               (unsigned int)app_counter_get(),
               (int)app_breath_get_duty());
    }

    app_display_task();                 /* ★ 第 7 步新增：屏幕刷新（内部 10Hz 节流）*/
}
