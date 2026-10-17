/* ======================== 【为什么要有这个文件】 ========================  * 任务 8 要保存数据并能读回来。若参数散落在各模块的 static 变量里，  * 就没有一个东西可以整块写进 Flash。所以定义 g_param：一个结构体装下所有  * 需要掉电保持的量，写=整块写，读=整块读。  *  * 为什么结构体要带 magic/version/size/checksum：Flash 上电后可能是空的，  * 也可能残留上一次固件的旧格式。magic 认是不是我写的，version/size 认  * 格式对不对，checksum 认有没有写坏，缺一个都可能把乱码当参数用。  *  * 为什么定义在 .c、在 .h 里 extern：C 里跨文件共享变量只能这样（一处分配  * 内存，多处声明）。在头文件里定义会让每个 include 它的 .c 各存一份副本。  *  * 为什么只在按保存时才擦写：Flash 寿命约 10 万次，且逐飞 flash_* 内部  * 会关全局中断 30~60ms。所以运行秒数这类量只改内存，显式 save 才落盘。  * ====================================================================== */ 
 #include "app_config.h"
#include "app_param.h"
#include "app_led.h"
#include "app_tick.h"
#include "app.h"

#define APP_PARAM_WORDS         (sizeof(app_param_struct) / 4)

app_param_struct g_param;

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     恢复默认参数
 *-----------------------------------------------------------------------------------------------------------------*/
void app_param_reset(void)
{
    g_param.magic         = APP_PARAM_MAGIC;
    g_param.version       = APP_PARAM_VERSION;
    g_param.size          = (uint16)sizeof(app_param_struct);
    g_param.led_blink_ms  = 500;
    g_param.buzzer_freq   = APP_BUZZER_FREQ_DEFAULT;
    g_param.breath_step   = APP_BREATH_STEP;
    g_param.save_count    = 0;
    g_param.run_seconds   = 0;
    g_param.checksum      = 0;
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     计算校验和
 * 备注信息     累加除最后一个字段(checksum 自己)以外的所有 32 位字
 *-----------------------------------------------------------------------------------------------------------------*/
static uint32 app_param_checksum(void)
{
    const uint32 *word = (const uint32 *)&g_param;
    uint32 sum = 0;
    uint8  i;

    for(i = 0; i < (APP_PARAM_WORDS - 1); i ++)
    {
        sum += word[i];
    }

    return sum;
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     上电加载参数
 * 备注信息     三种失败情形(空白/魔数不对/校验不过)统一退回默认值 —— 保证程序永远能跑起来
 *-----------------------------------------------------------------------------------------------------------------*/
void app_param_load(void)
{
    uint32 tick_before = app_tick_get();            /* 用于测量读取耗时, 可删 */

    if(0 == flash_check(APP_FLASH_SECTOR, APP_FLASH_PAGE))      /* 0 = 空白, 从来没存过 */
    {
        printf("[PARAM] flash blank -> default\r\n");
        app_param_reset();
        return;
    }

    /* 直接从 Flash 读进结构体: len 单位是"字", 不是字节! */
    flash_read_page(APP_FLASH_SECTOR, APP_FLASH_PAGE, (uint32 *)&g_param, (uint16)APP_PARAM_WORDS);

    if((APP_PARAM_MAGIC != g_param.magic) ||
       ((uint16)sizeof(app_param_struct) != g_param.size) ||
       (app_param_checksum() != g_param.checksum))
    {
        printf("[PARAM] data invalid (magic=0x%08X size=%d) -> default\r\n",
               (int)g_param.magic, (int)g_param.size);
        app_param_reset();
    }
    else
    {
        printf("[PARAM] loaded ok, save_count=%d, run_seconds=%d\r\n",
               (int)g_param.save_count, (int)g_param.run_seconds);
    }

    printf("[PARAM] read cost %d ms\r\n", (int)(app_tick_get() - tick_before));
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     保存参数到 FLASH
 * 返回参数     0 = 成功, 1 = 失败
 * 备注信息     1) flash_write_page 内部会自己判断"有数据先擦除"
 *              2) 擦写期间全局中断被关闭, 系统节拍会停; 这里量出来给大家看
 *-----------------------------------------------------------------------------------------------------------------*/
uint8 app_param_save(void)
{
    uint8  ret;
    uint32 tick_before;

    g_param.magic       = APP_PARAM_MAGIC;
    g_param.version     = APP_PARAM_VERSION;
    g_param.size        = (uint16)sizeof(app_param_struct);
    g_param.save_count ++;                      /* 每保存一次 +1, 断电重启能看出来 */
    g_param.run_seconds = app_tick_get() / 1000;
    g_param.checksum    = app_param_checksum();

    tick_before = app_tick_get();

    printf("[PARAM] writing sector=%d page=%d, %d words ...\r\n",
           APP_FLASH_SECTOR, APP_FLASH_PAGE, (int)APP_PARAM_WORDS);

    ret = flash_write_page(APP_FLASH_SECTOR, APP_FLASH_PAGE, (const uint32 *)&g_param, (uint16)APP_PARAM_WORDS);

    printf("[PARAM] %s, cost %d ms (节拍中断被暂停的时长)\r\n",
           (ret) ? ("FAIL") : ("OK"), (int)(app_tick_get() - tick_before));

    return ret;
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     把参数下发到各个模块（改完参数后调用）
 *-----------------------------------------------------------------------------------------------------------------*/
void app_param_apply(void)
{
    app_led_blink_interval(g_param.led_blink_ms);
    app_breath_set_step((int32)g_param.breath_step);            /* 见 §8.5 */
}

void app_param_show(void)
{
    printf("---- app parameters ----\r\n");
    printf("  version     : %d.%d\r\n", (int)(g_param.version >> 8), (int)(g_param.version & 0xFF));
    printf("  size        : %d bytes\r\n", (int)g_param.size);
    printf("  led_blink_ms: %d\r\n", (int)g_param.led_blink_ms);
    printf("  buzzer_freq : %d Hz\r\n", (int)g_param.buzzer_freq);
    printf("  breath_step : %d (0~10000)\r\n", (int)g_param.breath_step);
    printf("  save_count  : %d\r\n", (int)g_param.save_count);
    printf("  run_seconds : %d\r\n", (int)g_param.run_seconds);
    printf("  checksum    : 0x%08X\r\n", (int)g_param.checksum);
    printf("------------------------\r\n");
}
