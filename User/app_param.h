/* 参数结构体与存取接口。g_param 的内存定义在 app_param.c 里。 */
#ifndef _app_param_h_
#define _app_param_h_

#include "zf_common_headfile.h"

/* 参数结构体 —— 布局一旦发布就不要随意增删字段, 要加就放到最后并升 version */
typedef struct
{
    uint32 magic;                   /* 固定 APP_PARAM_MAGIC                  */
    uint16 version;                 /* 结构体版本                            */
    uint16 size;                    /* sizeof(app_param_struct) 字节数        */

    /* ---------------- 真正的参数 ---------------- */
    uint32 led_blink_ms;            /* LED 闪烁周期 (ms)                      */
    uint32 buzzer_freq;             /* 无源蜂鸣器频率 (Hz)                    */
    uint32 breath_step;             /* 呼吸灯每圈步长 (0~10000)               */
    uint32 save_count;              /* 保存次数(用于证明数据真的写进去了)     */
    uint32 run_seconds;             /* 上次退出/保存时的累计运行秒数          */

    uint32 checksum;                /* 前面所有 32 位字的累加和  ※必须是最后一个字段 */
} app_param_struct;

extern app_param_struct g_param;

void  app_param_load(void);                     /* 上电读取(失败则用默认值) */
uint8 app_param_save(void);                     /* 保存并返回 0=成功 1=失败 */
void  app_param_reset(void);                    /* 恢复默认值(不写 Flash)   */
void  app_param_apply(void);                    /* 把参数下发到各模块       */
void  app_param_show(void);                     /* 串口打印当前参数         */

#endif
