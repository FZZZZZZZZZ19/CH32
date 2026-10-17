/* ======================== 【为什么要有这个文件】 ========================  * 任务 3/6 要点灯，第 10 步的菜单也要点灯。若每次都直接写 gpio_set_level()，  * 灯的状态会到处不一致，将来加个闪烁要改十几处。这里把「点亮」这件事收口：  * 软件记住每一路状态，闪烁/流水统一由 app_led_task() 推进。  *  * 为什么 index 0 不能当普通灯用：index 0 = PA11 = TIM1_CH4 的 PWM 输出  * （呼吸灯）。一根引脚被配成 PWM 后，再用 gpio_set_level() 写它没有意义。  * 所以普通点灯、二进制计数、流水灯都只用 index 1~3。  * ====================================================================== */ 
 #include "app_config.h"
#include "app_led.h"
#include "app_tick.h"          /* [FIX] 文档 6.4 未给出：app_led_task() 用了 app_tick_reached() */

/* 软件编号 -> 实际引脚。改引脚只改这张表的顺序/内容即可（配合 app_config.h 的注释） */
static const gpio_pin_enum app_led_pin[APP_LED_NUM] =
{
    A11,        /* 0: LED8 */
    A12,        /* 1: LED7 */
    A15,        /* 2: LED6 */
    D1,         /* 3: LED5 */
};

static uint8 app_led_state[APP_LED_NUM] = {0, 0, 0, 0};

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     LED 初始化
 * 参数说明     void
 * 返回参数     void
 * 备注信息     共阴接法 -> 输出低电平即熄灭，所以初始电平传 APP_LED_OFF_LEVEL
 *-----------------------------------------------------------------------------------------------------------------*/
void app_led_init(void)
{
    uint8 i;

    for(i = 0; i < APP_LED_NUM; i ++)
    {
        /* gpio_init(引脚, 方向, 初始电平, 模式) ---- 注意是 4 个参数 */
        gpio_init(app_led_pin[i], GPO, APP_LED_OFF_LEVEL, GPIO_PIN_CONFIG);
        app_led_state[i] = 0;
    }
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     设置单个 LED
 * 备注信息     带越界保护 —— 传错下标不会写坏别的内存，只会什么都不做
 *-----------------------------------------------------------------------------------------------------------------*/
void app_led_set(uint8 index, uint8 on)
{
    if(index >= APP_LED_NUM)
    {
        return;
    }

    gpio_set_level(app_led_pin[index], (on) ? (APP_LED_ON_LEVEL) : (APP_LED_OFF_LEVEL));
    app_led_state[index] = (on) ? (1) : (0);
}

void app_led_toggle(uint8 index)
{
    if(index >= APP_LED_NUM)
    {
        return;
    }
    gpio_toggle_level(app_led_pin[index]);
    app_led_state[index] = (app_led_state[index]) ? (0) : (1);
}

void app_led_all(uint8 on)
{
    uint8 i;
    for(i = 0; i < APP_LED_NUM; i ++)
    {
        app_led_set(i, on);
    }
}

uint8 app_led_get(uint8 index)
{
    if(index >= APP_LED_NUM)
    {
        return 0;
    }
    return app_led_state[index];
}

static uint8  led_blink_mask = 0;                   /* bit i = 1 表示第 i 路要闪 */
static uint32 led_blink_ms   = 500;
static uint32 led_blink_last = 0;

void app_led_blink_set(uint8 index, uint8 enable)
{
    if(index >= APP_LED_NUM)
    {
        return;
    }

    if(enable)
    {
        led_blink_mask |=  (uint8)(1 << index);
    }
    else
    {
        led_blink_mask &= (uint8)(~(1 << index));
    }
}

void app_led_blink_interval(uint32 ms)
{
    if(ms >= 10)                    /* 保护：不要小于一个节拍，否则看起来是常亮 */
    {
        led_blink_ms = ms;
    }
}

static uint8  led_chase_on  = 0;                    /* 流水灯开关 */
static uint8  led_chase_pos = APP_LED_BIN_FIRST;    /* 位置：从第 1 路开始（第 0 路是 PWM 呼吸灯）*/

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     LED 后台任务：只负责"到点了就把闪烁组里的 LED 翻转"
 * 备注信息     由 app_tick 提供时间基准，不占用中断
 *-----------------------------------------------------------------------------------------------------------------*/
void app_led_task(void)
{
    uint8 i;

    if(0 == app_tick_reached(&led_blink_last, led_blink_ms))
    {
        return;
    }

    for(i = 0; i < APP_LED_NUM; i ++)
    {
        if(led_blink_mask & (uint8)(1 << i))
        {
            app_led_toggle(i);
        }
    }

    /* 流水灯轮转：只在第 1~3 路之间转，绝不动第 0 路（那一路交给了 TIM1 的 PWM）*/
    if(led_chase_on)
    {
        app_led_set(led_chase_pos, 0);
        led_chase_pos = (uint8)(APP_LED_BIN_FIRST
                       + ((led_chase_pos - APP_LED_BIN_FIRST + 1) % APP_LED_BIN_NUM));
        app_led_set(led_chase_pos, 1);
    }
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     流水灯 开/关
 * 备注信息     ★ 只使用第 1~3 路：第 0 路(A11/PA11) 已经被 TIM1_CH4 的 PWM 占用，
 *              再当普通 GPIO 去写它会把 PWM 输出打乱。
 *-----------------------------------------------------------------------------------------------------------------*/
void app_led_chase_toggle(void)
{
    if(led_chase_on)
    {
        led_chase_on = 0;
        app_led_set(led_chase_pos, 0);
    }
    else
    {
        led_chase_on  = 1;
        led_chase_pos = APP_LED_BIN_FIRST;
        app_led_set(led_chase_pos, 1);
    }
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     LED 是否被"动画"占用（闪烁组 或 流水灯）
 * 备注信息     ★ 10Hz 的二进制计数和它们抢的是同一批 LED，所以计数要让路：
 *              app.c 里只有在 app_led_is_busy()==0 时才刷新计数。
 *-----------------------------------------------------------------------------------------------------------------*/
uint8 app_led_is_busy(void)
{
    return (uint8)((led_blink_mask != 0) || (led_chase_on != 0));
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     用连续的 APP_LED_BIN_NUM 路 LED 显示一个二进制数
 * 参数说明     value  0 ~ (2^APP_LED_BIN_NUM - 1)
 * 备注信息     ★ 第 0 路（PA11）已经交给 PWM 呼吸灯了，所以本函数从第 1 路开始用
 *-----------------------------------------------------------------------------------------------------------------*/
void app_led_show_bin(uint8 value)
{
    uint8 i;

    for(i = 0; i < APP_LED_BIN_NUM; i ++)
    {
        app_led_set((uint8)(APP_LED_BIN_FIRST + i), (value >> i) & 0x01);
    }
}
