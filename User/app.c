/********************************************************************************
* File Name          : app.c
* Description        : 应用层总调度 —— 第 10 步版（+ 三级菜单 + 参数历史记录）
*
* 【这一步在干什么】
*   1) 菜单：把前面 9 步的功能全部挂成一棵树，用五向开关 + 两个按键在里面走。
*      第 1 级是主菜单，第 2 级是功能页，第 3 级是能改数值的参数编辑页
*      —— "三级菜单"这个任务要求的就是这三层，而不是三个页面。
*   2) 记录：每次保存参数时，往 Flash 的另一个扇区追加一条历史（扇区 62），
*      于是"改过什么、什么时候改的"能查 —— 这是任务 10 里的"记录参数"。
*
* 【为什么菜单和记录要单独两个文件】
*   菜单是"界面 + 按键"，记录是"数据 + Flash"，两者的生命周期完全不一样：
*   界面以后可能整个换掉（比如换成图形界面），记录格式几乎不会变。
********************************************************************************/
#include "app_config.h"
#include "app.h"
#include "app_led.h"
#include "app_buzzer.h"
#include "app_tick.h"
#include "app_display.h"
#include "app_param.h"
#include "app_console.h"
#include "app_key.h"
#include "app_ui.h"
#include "app_menu.h"               /* 第 10 步新增：三级菜单状态机 */
#include "app_log.h"                /* 第 10 步新增：参数历史记录   */

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
static uint32 t_key   = 0;

/* ============================== 上电初始化 ==============================
 * 【为什么 app_log_init() 必须排在 app_menu_init() 前面】
 *   菜单首页要显示"已保存记录：N 条"，N 来自 app_log_init() 读出来的记录数。
 *   顺序反了首页就会显示 0 条，直到退出再进才刷新 —— 这种 bug 很难查。
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
    /* app_display_init() 不在这里调用了：从第 10 步起整块屏幕归菜单（app_menu + app_ui），
       数据页和菜单的行坐标重叠，两边都画会互相覆盖。app_display.c 仍然参与编译，
       想在菜单里单独看数据页的话，在菜单表里加一项调 app_display_init() 即可。 */

    app_key_init();
    app_log_init();                     /* ★ 第 10 步：先读历史记录（扇区 62）  */
    app_menu_init();                    /* ★ 第 10 步：再进菜单，首页就能显示条数 */

    app_ui_init();
    app_console_init();
    app_param_show();

    printf("[APP] step10: + 3-level menu + param log\r\n");
}

/* ================================ 主循环 ================================
 * 【为什么 app_menu_task() 放在最后】
 *   它要消费按键事件。事件是"取一次就清掉"的，所以必须让前面所有可能
 *   也想看按键的东西（这里没有）先处理完，最后才由菜单统一消费。
 * --------------------------------------------------------------------*/
void app_task(void)
{
    app_led_task();
    app_buzzer_task();
    app_breath_task();

    if(app_tick_reached(&t_key, 10))
    {
        app_key_task();                 /* 扫描 + 消抖 + 生成事件        */
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

    app_ui_task();                      /* 临时消息倒计时                */
    app_console_task();                 /* 串口控制台（不阻塞）          */
    app_menu_task();                    /* ★ 第 10 步：消费按键 + 重画菜单 */
    /* app_display_task() 同样不调：数字会写到菜单的行上，把字盖掉。 */
}
