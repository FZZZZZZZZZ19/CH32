/********************************************************************************
* File Name          : app.c
* Description        : 应用层总调度 —— 第 5 步版（PWM：无源蜂鸣器 + 呼吸灯）
*
* 【为什么要有这个文件】
*   main.c 只做三件事：配时钟、开调试串口、把控制权交给 app_init()/app_task()。
*   所有业务代码都在 app 层 —— 以后加功能只改 app_*.c，main.c 一行都不用动。
*   答辩讲"代码结构"时，这就是最外层的分层。
*
* 【这一步为什么这样写】
*   第 5 步还没有定时器中断（那是第 6 步的事），所以先用 system_delay_ms()
*   阻塞延时把现象跑出来：看见灯亮、听见蜂鸣器响、看见呼吸灯渐变。
*   第 6 步会把这里的延时全部换成"1ms 节拍 + 非阻塞判断"。
********************************************************************************/
#include "app_config.h"
#include "app.h"
#include "app_led.h"
#include "app_buzzer.h"

/* ================================ 呼吸灯 ================================
 * 【为什么放在 app.c 里、不单独建文件】
 *   它只有 1 个 PWM 通道 + 3 个变量，单独建文件反而多一层跳转；
 *   等它需要被外部调用（第 8 步的菜单要改它的步长）时再暴露接口。
 * ----------------------------------------------------------------------*/
static int32 breath_duty = 0;                       /* 当前占空比 0~10000      */
static int32 breath_dir  = 1;                       /* +1 变亮 / -1 变暗       */
static int32 breath_step = APP_BREATH_STEP;         /* 每圈变化量              */

static void app_breath_init(void)
{
    /* pwm_init(通道, 频率, 初始占空比)
     *   APP_BREATH_PWM = TIM1_PWM_MAP0_CH4_A11
     *   蜂鸣器         = TIM1_PWM_MAP0_CH1_A8
     * 两者同属 TIM1、同一组映射(MAP0)，所以可以同时存在：
     * 频率必须相同，占空比各自独立。 */
    pwm_init(APP_BREATH_PWM, APP_BUZZER_FREQ_DEFAULT, 0);
}

void app_breath_set_step(int32 step)
{
    if(step < 1)    { step = 1;    }
    if(step > 1000) { step = 1000; }                /* 太大了看起来就成闪烁了 */
    breath_step = step;
}

/* 【为什么叫 task 却这么小】它是"主循环每调用一次就往前走一小步"的写法，
 * 不阻塞、不等待。这样第 6 步把主循环换成节拍调度时，它一行都不用改。 */
static void app_breath_task(void)
{
    breath_duty += breath_dir * breath_step;

    if(breath_duty >= PWM_DUTY_MAX) { breath_duty = PWM_DUTY_MAX; breath_dir = -1; }
    else if(breath_duty <= 0)       { breath_duty = 0;            breath_dir =  1; }

    pwm_set_duty(APP_BREATH_PWM, (uint32)breath_duty);
}

int32 app_breath_get_duty(void) { return breath_duty; }
uint8 app_counter_get(void)     { return 0; }       /* 计数器是第 6 步才有的东西 */

/* ============================== 上电初始化 ============================== */
void app_init(void)
{
    app_led_init();                 /* 4 路 LED 配成推挽输出，默认全灭      */
    app_buzzer_init();              /* PA8  -> TIM1_CH1 PWM                 */
    app_breath_init();              /* PA11 -> TIM1_CH4 PWM                 */

    printf("[APP] step5: led + pwm (buzzer / breath) ready\r\n");
}

/* ================================ 主循环 ================================
 * 第 5 步还没有节拍，就用一次阻塞延时控制"每轮间隔 100ms"。
 * --------------------------------------------------------------------*/
void app_task(void)
{
    static uint8 pos = APP_LED_BIN_FIRST;

    /* 跑马灯：只在第 1~3 路之间跑。
     * ★ 第 0 路(A11/PA11) 已经被 TIM1_CH4 拿去呼吸了，不能再当普通 GPIO 写。 */
    app_led_set(pos, 0);
    pos = (uint8)(APP_LED_BIN_FIRST
                + ((pos - APP_LED_BIN_FIRST + 1) % APP_LED_BIN_NUM));
    app_led_set(pos, 1);

    app_buzzer_beep(APP_BUZZER_FREQ_DEFAULT, 30);   /* 阻塞 30ms 响一声          */
    app_breath_task();                              /* 呼吸灯往前走一步          */

    system_delay_ms(100);                           /* 第 6 步会用 1ms 节拍替换掉 */
}
