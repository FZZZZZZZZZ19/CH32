/* 参数历史记录对外接口（另一个扇区，与参数区互不影响）。 */
#ifndef _app_log_h_
#define _app_log_h_

#include "zf_common_headfile.h"

/* 一条参数记录 */
typedef struct
{
    uint32 seq;                     /* 序号, 从 1 开始一直累加 */
    uint32 tick_s;                  /* 保存时系统已运行秒数 */
    uint32 led_blink_ms;
    uint32 buzzer_freq;
    uint32 breath_step;
    uint32 checksum;                /* 本条记录的累加和 */
} app_log_entry_struct;

void                   app_log_init(void);      /* 上电加载(失败则清零) */
uint8                  app_log_append(void);    /* 追加一条当前参数, 0=成功 */
void                   app_log_clear(void);     /* 清空全部记录 */
uint32                 app_log_count(void);     /* 总条数 */
const app_log_entry_struct * app_log_get(uint8 index);  /* index 0 = 最新 */
void                   app_log_show(void);      /* 串口打印全部记录 */

#endif
