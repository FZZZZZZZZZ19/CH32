/********************************************************************************
* File Name          : app.c
* Description        : 应用层总调度 —— 第 6 步版（1ms 定时器中断 + 非阻塞调度）
*
* 【这一步在干什么】
*   第 5 步的主循环里有一个 system_delay_ms(100)：主循环被"卡"在那里 100ms。
*   这一步把"时间"这件事交给 TIM6：1ms 中断一次，中断里只把 g_app_tick_ms 加一。
*   主循环变成"每次跑到就检查一下时间到了没有"，于是不再有任何阻塞。
*
* 【为什么中断函数不在这个文件里】
*   中断入口（TIM6_IRQHandler）集中在 User/zf_isr.c —— 一个文件看全所有中断，
*   排优先级、查冲突都方便。这里只提供被它调用的 app_tick_irq()。
********************************************************************************/
#include "app_config.h"
#include "app.h"
#include "app_led.h"
#include "app_buzzer.h"
#include "app_tick.h"               /* 第 6 步新增：1ms 节拍 */

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

/* ============================== 二进制计数 ==============================
 * 【为什么要有它】任务 6 要"定时中断控制 LED"。这里用 1ms 节拍每 100ms 加一次
 * 计数器，再把低 3 位显示到 3 路 LED 上 —— 灯在按二进制递增，就是最直观的证据。
 * --------------------------------------------------------------------*/
static uint32 counter = 0;

uint8 app_counter_get(void) { return (uint8)(counter & 0x07); }

/* =========================== 非阻塞定时的时间戳 ===========================
 * 【为什么是 static uint32】每个"每 N 毫秒做一次"的事情都记住自己上次做的时间，
 * app_tick_reached() 负责比较。它们必须是静态的（跨调用保持），也必须是
 * 无符号的（app_tick_reached 内部用无符号减法，回绕也不会算错）。
 * ----------------------------------------------------------------------*/
static uint32 t_count = 0;          /* 计数：每 100 ms 加一   */
static uint32 t_beep  = 0;          /* 叫一声：每 1000 ms     */
static uint32 t_print = 0;          /* 串口打印：每 500 ms    */

/* ============================== 上电初始化 ============================== */
void app_init(void)
{
    app_led_init();
    app_buzzer_init();
    app_breath_init();

    /* ★ 放在最后：先把外设配好，再打开 1ms 中断开始计时 */
    app_tick_init();

    printf("[APP] step6: 1ms tick + non-blocking loop ready\r\n");
}

/* ================================ 主循环 ================================
 * 【这一段为什么没有一个延时】
 *   所有"等一会儿"都换成了"时间到了没有"。主循环一圈只跑几十微秒，
 *   所以第 9 步的按键扫描、第 10 步的菜单重画都能塞进来而互相不卡。
 * --------------------------------------------------------------------*/
void app_task(void)
{
    /* --- 后台任务：有活干一点，立刻返回 --- */
    app_led_task();                     /* 闪烁组 / 流水灯（本步还没用，第 10 步的菜单会用）*/
    app_buzzer_task();                  /* 给 app_buzzer_start() 的"响 N 毫秒"收尾 */
    app_breath_task();                  /* 呼吸灯往前走一步 */

    /* --- 100ms：二进制计数上灯（任务 6 的主演示）--- */
    if(app_tick_reached(&t_count, 100))
    {
        counter ++;

        /* 【为什么要判断 busy】闪烁组和流水灯抢的是同一批 LED，
         * 有动画在用时就让路，否则两边互相覆盖，看着像坏了。 */
        if(0 == app_led_is_busy())
        {
            app_led_show_bin(app_counter_get());
        }
    }

    /* --- 1000ms：叫一声 --- */
    if(app_tick_reached(&t_beep, 1000))
    {
        app_buzzer_start(APP_BUZZER_FREQ_DEFAULT, 50);      /* 非阻塞：50ms 后自动停 */
    }

    /* --- 500ms：串口报一次，证明节拍真的在走 --- */
    if(app_tick_reached(&t_print, 500))
    {
        printf("[APP] tick=%u s, cnt=%u\r\n",
               (unsigned int)(app_tick_get() / 1000), (unsigned int)app_counter_get());
    }
}
