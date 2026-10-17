/********************************************************************************
* File Name          : app.c
* Description        : 应用层总调度 —— 第 8 步版（+ Flash 参数保存 + 串口控制台）
*
* 【这一步在干什么】
*   1) 参数不再写死在代码里，而是放在 g_param 结构体里，上电从 Flash 读、改完写回，
*      断电重启后还在 —— 这是任务 8 的全部内容；
*   2) 加一个串口控制台：不重新烧程序就能改参数、保存、查看记录。
*      它是任务 11「不重新烧录完成所有任务」的技术底座。
*
* 【为什么 g_param 定义在 app_param.c 而不是这里】
*   因为除了 app.c，菜单（第 10 步）和控制台（本步）都要读写同一份参数，
*   C 里跨文件共享只有一个办法：在 .c 里定义、在 .h 里 extern。
********************************************************************************/
#include "app_config.h"
#include "app.h"
#include "app_led.h"
#include "app_buzzer.h"
#include "app_tick.h"
#include "app_display.h"
#include "app_param.h"              /* 第 8 步新增：g_param + 存取 */
#include "app_console.h"            /* 第 8 步新增：串口命令行   */

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
static uint32 t_run   = 0;          /* 累计运行秒数（第 8 步新增）            */

/* ============================== 上电初始化 ==============================
 * 【为什么顺序不能乱】
 *   1) LED / 蜂鸣器 / 呼吸灯：先把"被控制的对象"配好；
 *   2) app_param_load()：从 Flash 把参数读进内存（失败就用默认值，不会卡死）；
 *   3) app_param_apply()：把参数下发到上面三个模块。
 *      ★ 必须放在 1) 之后 —— 下发时改的是模块内部变量，
 *        模块还没初始化就下发，等于对着空气下命令。
 *   4) 节拍 / 屏幕 / 控制台：最后起。
 * --------------------------------------------------------------------*/
void app_init(void)
{
    app_led_init();
    app_buzzer_init();
    app_breath_init();

    app_param_load();                   /* ★ 第 8 步：从 Flash 读参数         */
    app_param_apply();                  /* ★ 第 8 步：下发到各模块            */
    app_breath_set_step((int32)g_param.breath_step);    /* 呼吸灯步长是 static，这里兜底 */

    app_tick_init();
    app_display_init();
    app_console_init();                 /* ★ 第 8 步：串口控制台就绪          */

    app_param_show();                   /* 上电把参数打一遍，方便对照          */
    printf("[APP] step8: + flash param + console\r\n");
}

/* ================================ 主循环 ================================ */
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
        /* 【为什么这里用 g_param.buzzer_freq，而不是第 5 步那个写死的宏】
         * 因为这个值是从 Flash 里读出来的。你在串口控制台敲一条命令改掉它并 save，
         * 下次开机蜂鸣器就是新频率 —— 全程没有重新烧程序（任务 11 的核心）。 */
        app_buzzer_start(g_param.buzzer_freq, 50);
    }

    if(app_tick_reached(&t_run, 1000))
    {
        /* 运行秒数只改内存，不写 Flash！
         * 【为什么】Flash 擦写寿命约 10 万次，每秒写一次几小时就废了。
         * 只在用户显式执行 save 时落盘。 */
        g_param.run_seconds ++;
    }

    if(app_tick_reached(&t_print, 500))
    {
        printf("[APP] tick=%u s, cnt=%u\r\n",
               (unsigned int)(app_tick_get() / 1000), (unsigned int)app_counter_get());
    }

    app_display_task();                 /* 屏幕 10Hz 刷新（内部节流）          */
    app_console_task();                 /* ★ 第 8 步：解析串口命令（不阻塞）   */
}
