/* ======================== 【为什么要有这个文件】 ========================  * 任务 11 要求不重新烧录程序完成所有任务。对参数类的东西，最直接的实现  * 就是一条串口命令：改值 -> 保存 -> 断电重启依然生效。现场演示也最好讲。  *  * 为什么不用阻塞接收（scanf 式）：那会让主循环停在那里等字符，屏幕和按键  * 全部失去响应。这里用每圈取一次接收缓冲、攒够一行再解析的非阻塞写法。  * 为什么自己写行解析：工程里没有 console 库，逐飞 debug 模块只给了发送接口和  * 一个环形缓冲（debug_read_ring_buffer），拿它拼几十行解析器最省事。  * ====================================================================== */ 
 #include "app_config.h"
#include "app_console.h"
#include "app_param.h"
#include "app_tick.h"
#include "app_buzzer.h"
#include "app_log.h"            /* [FIX] 文档 10.6 的 "log" 命令要打印参数记录 */

#define CONSOLE_LINE_LEN            (32)

static char    line_buf[CONSOLE_LINE_LEN];
static uint8   line_len = 0;

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     命令行初始化
 * [FIX] 文档 9.5 / 10.6 的 app_init() 都调用了 app_console_init(), 而 app_console.h
 *       也声明了它, 但步骤 8 从未给出定义 —— 链接期 undefined reference。
 *-----------------------------------------------------------------------------------------------------------------*/
void app_console_init(void)
{
    line_len = 0;
    line_buf[0] = '\0';
    printf("[CONS] console ready, type 'help'\r\n");
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     把一行命令拆成 3 个参数
 *-----------------------------------------------------------------------------------------------------------------*/
static uint8 app_console_parse(char *line, char *cmd, char *arg1, char *arg2)
{
    char *token;
    uint8 index = 0;

    cmd[0] = arg1[0] = arg2[0] = '\0';

    token = strtok(line, " \t");
    while(NULL != token)
    {
        switch(index)
        {
            case 0: strncpy(cmd,  token, 15); cmd[15]  = '\0'; break;
            case 1: strncpy(arg1, token, 15); arg1[15] = '\0'; break;
            case 2: strncpy(arg2, token, 15); arg2[15] = '\0'; break;
            default: break;
        }
        index ++;
        token = strtok(NULL, " \t");
    }

    return index;
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     执行一条命令
 *-----------------------------------------------------------------------------------------------------------------*/
static void app_console_exec(char *line)
{
    char cmd[16], arg1[16], arg2[16];

    if(0 == app_console_parse(line, cmd, arg1, arg2))
    {
        return;
    }

    /* ------------ 帮助 ------------ */
    if(0 == strcmp(cmd, "help"))
    {
        printf("commands:\r\n");
        printf("  help                    显示本帮助\r\n");
        printf("  ver                     查看版本\r\n");
        printf("  param show              打印当前参数\r\n");
        printf("  param save              保存参数到 FLASH\r\n");
        printf("  param load              从 FLASH 重新加载\r\n");
        printf("  param reset             恢复默认并保存\r\n");
        printf("  set blink <ms>          设置 LED 闪烁周期\r\n");
        printf("  set freq <Hz>           设置蜂鸣器频率\r\n");
        printf("  set step <n>            设置呼吸灯步长\r\n");
        printf("  beep [ms]               蜂鸣器鸣叫(默认100ms)\r\n");
        printf("  stamp <text>            打时间戳\r\n");
        printf("  log                     打印参数记录\r\n");
    }
    /* ------------ 版本 ------------ */
    else if(0 == strcmp(cmd, "ver"))
    {
        printf("app %s, build %s %s\r\n", APP_VERSION_STR, __DATE__, __TIME__);
    }
    /* ------------ 参数 ------------ */
    else if(0 == strcmp(cmd, "param"))
    {
        if(0 == strcmp(arg1, "show"))
        {
            app_param_show();
        }
        else if(0 == strcmp(arg1, "save"))
        {
            app_param_save();
        }
        else if(0 == strcmp(arg1, "load"))
        {
            app_param_load();
            app_param_apply();
        }
        else if(0 == strcmp(arg1, "reset"))
        {
            app_param_reset();
            app_param_save();               /* reset 后立刻落盘 */
            app_param_apply();
            printf("[CONSOLE] parameters restored to default\r\n");
        }
        else
        {
            printf("[CONSOLE] usage: param show|save|load|reset\r\n");
        }
    }
    /* ------------ 改参数 ------------ */
    else if(0 == strcmp(cmd, "set"))
    {
        if(0 == strcmp(arg1, "blink"))
        {
            g_param.led_blink_ms = (uint32)atoi(arg2);
            app_param_apply();
            printf("[CONSOLE] led_blink_ms = %d (记得 param save)\r\n", (int)g_param.led_blink_ms);
        }
        else if(0 == strcmp(arg1, "freq"))
        {
            g_param.buzzer_freq = (uint32)atoi(arg2);
            printf("[CONSOLE] buzzer_freq = %d (记得 param save)\r\n", (int)g_param.buzzer_freq);
        }
        else if(0 == strcmp(arg1, "step"))
        {
            g_param.breath_step = (uint32)atoi(arg2);
            app_param_apply();
            printf("[CONSOLE] breath_step = %d (记得 param save)\r\n", (int)g_param.breath_step);
        }
        else
        {
            printf("[CONSOLE] usage: set blink|freq|step <value>\r\n");
        }
    }
    /* ------------ 蜂鸣器 ------------ */
    else if(0 == strcmp(cmd, "beep"))
    {
        uint32 ms = (arg1[0]) ? ((uint32)atoi(arg1)) : (100);
        app_buzzer_start(g_param.buzzer_freq, ms);
        printf("[CONSOLE] beep %d Hz for %d ms\r\n", (int)g_param.buzzer_freq, (int)ms);
    }
    /* ------------ 时间戳 ------------ */
    else if(0 == strcmp(cmd, "stamp"))
    {
        printf("[STAMP] tick=%d ms  |%s|\r\n", (int)app_tick_get(), arg1);
    }
    else if(0 == strcmp(cmd, "log"))            /* [FIX] 文档 10.6 追加的命令 */
    {
        app_log_show();
    }
    else
    {
        printf("[CONSOLE] unknown command '%s', type 'help'\r\n", cmd);
    }
}

/*-------------------------------------------------------------------------------------------------------------------
 * 函数简介     非阻塞的命令行任务
 * 备注信息     1) debug_read_ring_buffer 从库的 64 字节环形缓冲区取数据(中断已经填好)
 *              2) 只有收到 '\r' 或 '\n' 才执行 —— 这就是最简单的行缓冲
 *-----------------------------------------------------------------------------------------------------------------*/
void app_console_task(void)
{
    uint8  ch;
    uint8  i;
    uint32 got;

    for(i = 0; i < 8; i ++)
    {
        got = debug_read_ring_buffer(&ch, 1);       /* 读 1 字节, 没有则返回 0 */
        if(0 == got)
        {
            break;
        }

        if(('\r' == ch) || ('\n' == ch))
        {
            line_buf[line_len] = '\0';
            if(line_len > 0)
            {
                app_console_exec(line_buf);         /* 执行 */
            }
            line_len = 0;
        }
        else if((ch >= 0x20) && (ch < 0x7F))        /* 可打印字符才存 */
        {
            if(line_len < (CONSOLE_LINE_LEN - 1))
            {
                line_buf[line_len ++] = (char)ch;
            }
        }
        else
        {
            /* 其它控制字符忽略 */
        }
    }
}
