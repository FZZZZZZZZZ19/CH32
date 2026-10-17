/* ======================== 【为什么要有这个文件】 ========================  * 逐飞自带的 zf_device_key 是给 4 个独立 GPIO 按键写的：  * zf_device_key.h 里写死 KEY_LIST = {A8, D8, B12, B0}。可本板上 A8 是蜂鸣器、  * B0 是 UART4_TX，真正的按键是经 74HC148 编码成 3 根线（PD8/PB12/PB14）。  * 套用库驱动会同时踩占用蜂鸣器脚和读不到 A2 那一位两个坑。  *  * 为什么必须自己消抖：机械触点在按下/松开的几毫秒内电平会抖，不消抖就会出现  * 按一下被判成好几次。这里的做法是连续两次 10ms 采样一致才算稳定。  *  * 为什么事件取一次就清（单槽而不是队列）：菜单只需要知道刚刚发生了什么，  * 队列会带来溢出和延迟，对这个小系统是过度设计。  * 为什么中断版（EXTI）只置标志：中断里不消抖、不判断，真正的判断留给 10ms 任务。  * ====================================================================== */ 
 #include "app_config.h"
#include "app_key.h"
#include "app_tick.h"

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     读一次编码器, 返回 0~7
 *-----------------------------------------------------------------------------------------------------------------*/
uint8 app_key_get_raw_code(void)
{
    uint8 code = 0;

    if(gpio_get_level(APP_KEY_PIN_A2))  { code |= 0x04; }
    if(gpio_get_level(APP_KEY_PIN_A1))  { code |= 0x02; }
    if(gpio_get_level(APP_KEY_PIN_A0))  { code |= 0x01; }

    return code;
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     编码 -> 键名
 *-----------------------------------------------------------------------------------------------------------------*/
static app_key_enum app_key_decode(uint8 code)
{
    switch(code)
    {
        case 0:  return APP_KEY_KEY2;
        case 1:  return APP_KEY_MID;
        case 2:  return APP_KEY_RIGHT;
        case 3:  return APP_KEY_KEY1;
        case 4:  return APP_KEY_UP;
        case 5:  return APP_KEY_DOWN;
        case 6:  return APP_KEY_LEFT;
        default: return APP_KEY_NONE;       /* 7 = 无键 / USELESS, 区分不了, 当无键 */
    }
}

/*-------------------------------------------------------------------------------------------------------------------
 * 静态变量     按键状态机
 *-----------------------------------------------------------------------------------------------------------------*/
static app_key_enum       key_now          = APP_KEY_NONE;      /* 已消抖、当前按住的键 */
static app_key_enum       key_last_raw     = APP_KEY_NONE;      /* 上一次的原始值 */
static uint32             key_change_tick  = 0;                 /* 原始值最近一次变化的时刻 */
static uint32             key_press_tick   = 0;                 /* 确认按下的时刻 */
static uint8              key_long_sent    = 0;                 /* 长按事件是否已发过 */
static uint32             key_repeat_tick  = 0;
static app_key_event_enum key_event        = APP_KEY_EVENT_NONE;
static app_key_enum       key_event_key    = APP_KEY_NONE;

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     按键初始化
 *-----------------------------------------------------------------------------------------------------------------*/
void app_key_init(void)
{
    /* GPI + 内部上拉; 编码器输出是推挽的, 内部上拉只是双保险 */
    gpio_init(APP_KEY_PIN_A0, GPI, 0, GPI_PULL_UP);
    gpio_init(APP_KEY_PIN_A1, GPI, 0, GPI_PULL_UP);
    gpio_init(APP_KEY_PIN_A2, GPI, 0, GPI_PULL_UP);

    key_now    = app_key_decode(app_key_get_raw_code());
    key_last_raw = key_now;
    printf("[KEY] init, current code=%d\r\n", app_key_get_raw_code());
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     取一个按键事件(取完清空)
 *-----------------------------------------------------------------------------------------------------------------*/
app_key_event_enum app_key_get_event(app_key_enum *key)
{
    app_key_event_enum temp = key_event;

    if(NULL != key)
    {
        *key = key_event_key;
    }

    key_event     = APP_KEY_EVENT_NONE;
    key_event_key = APP_KEY_NONE;

    return temp;
}

app_key_enum app_key_get_pressed(void)
{
    return key_now;
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     按键扫描状态机, 每 APP_KEY_SCAN_MS 调一次
 * 备注信息     三级过滤: 1) 原始值稳定 2) 消抖时间 3) 长按/连发
 *-----------------------------------------------------------------------------------------------------------------*/
void app_key_task(void)
{
    app_key_enum raw = app_key_decode(app_key_get_raw_code());
    uint32       now = app_tick_get();

    /* ---------- 第 1 级: 原始值变了就重新计时 ---------- */
    if(raw != key_last_raw)
    {
        key_last_raw    = raw;
        key_change_tick = now;
        return;
    }

    /* ---------- 第 2 级: 稳定时间不足, 认为是抖动 ---------- */
    if((now - key_change_tick) < APP_KEY_DEBOUNCE_MS)
    {
        return;
    }

    /* ---------- 第 3 级: 稳定值相对"当前按住的键"发生变化 ---------- */
    if(raw == key_now)
    {
        /* 没换键 —— 检查长按与连发 */
        if(APP_KEY_NONE != key_now)
        {
            if((0 == key_long_sent) && ((now - key_press_tick) >= APP_KEY_LONG_MS))
            {
                key_long_sent   = 1;
                key_event       = APP_KEY_EVENT_LONG;
                key_event_key   = key_now;
                key_repeat_tick = now;
            }
            else if((key_long_sent) && ((now - key_repeat_tick) >= APP_KEY_REPEAT_MS))
            {
                key_repeat_tick = now;
                key_event       = APP_KEY_EVENT_REPEAT;
                key_event_key   = key_now;
            }
            else
            {
                /* 无事件 */
            }
        }
        return;
    }

    /* ---------- 换键了 ---------- */
    if(APP_KEY_NONE != raw)
    {
        /* 按下 */
        key_now         = raw;
        key_press_tick  = now;
        key_long_sent   = 0;
        key_event       = APP_KEY_EVENT_PRESS;
        key_event_key   = raw;
    }
    else
    {
        /* 松开 */
        key_now   = APP_KEY_NONE;
        key_event = APP_KEY_EVENT_NONE;
    }
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     给调试页用的键名
 *-----------------------------------------------------------------------------------------------------------------*/
const char * app_key_name(app_key_enum key)
{
    switch(key)
    {
        case APP_KEY_KEY1:  return "KEY1";
        case APP_KEY_KEY2:  return "KEY2";
        case APP_KEY_UP:    return "UP  ";
        case APP_KEY_DOWN:  return "DOWN";
        case APP_KEY_LEFT:  return "LEFT";
        case APP_KEY_RIGHT: return "RGHT";
        case APP_KEY_MID:   return "MID ";
        default:            return "----";
    }
}
