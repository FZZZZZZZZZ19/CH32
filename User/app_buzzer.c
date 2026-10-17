/* ======================== 【为什么要有这个文件】 ========================  * 无源蜂鸣器要的是「方波频率」而不是「通断」，所以它天生挂在 PWM 上。  * 独立成模块后，谁想让它响只需写 app_buzzer_beep(2700, 100)，  * 不必知道 PWM 通道号、占空比、以及「怎么停」。  *  * 为什么同时有阻塞版和非阻塞版：app_buzzer_beep() 原地延时，最直观（第 5 步用）；  * 第 6 步有了 1ms 节拍之后必须改用 start()+task()，否则主循环会被卡住。  * ====================================================================== */ 
 #include "app_config.h"
#include "app_buzzer.h"
#include "app_tick.h"

static uint8  buzzer_active  = 0;                   /* 正在发声                       */
static uint32 buzzer_stop_ms = 0;                   /* 该停下来的时刻（绝对毫秒）     */

void app_buzzer_init(void)
{
    /* 初始占空比 0：上电不响 */
    pwm_init(APP_BUZZER_PWM, APP_BUZZER_FREQ_DEFAULT, APP_BUZZER_DUTY_OFF);
}

void app_buzzer_on(uint32 freq)
{
    pwm_set_freq(APP_BUZZER_PWM, freq, APP_BUZZER_DUTY_ON);
}

void app_buzzer_off(void)
{
    pwm_set_duty(APP_BUZZER_PWM, APP_BUZZER_DUTY_OFF);
}

void app_buzzer_beep(uint32 freq, uint32 ms)
{
    app_buzzer_on(freq);
    system_delay_ms(ms);                            /* ※ 会阻塞 CPU，仅调试用 */
    app_buzzer_off();
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     非阻塞鸣叫：立刻开始响，ms 毫秒后由 app_buzzer_task() 自动关掉
 *-----------------------------------------------------------------------------------------------------------------*/
void app_buzzer_start(uint32 freq, uint32 ms)
{
    app_buzzer_on(freq);
    buzzer_active  = 1;
    buzzer_stop_ms = app_tick_get() + ms;
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     蜂鸣器后台任务（主循环里调用，不会阻塞）
 * 备注信息     用 int32 相减判"过没过期"，能正确处理毫秒计数器回绕
 *-----------------------------------------------------------------------------------------------------------------*/
void app_buzzer_task(void)
{
    if(buzzer_active)
    {
        if((int32)(app_tick_get() - buzzer_stop_ms) >= 0)
        {
            buzzer_active = 0;
            app_buzzer_off();
        }
    }
}
