/* ======================== 【为什么要有这个文件】 ========================  * 任务 10 要记录参数。光有当前值不够 —— 还要能回答第 3 次保存时蜂鸣器  * 频率是多少。历史记录和当前值用途不同，所以放两个扇区：参数区被频繁覆盖，  * 也不会把历史冲掉。  *  * 为什么整块读写：Flash 最小擦除单位是 4KB（一个扇区），不能只改一条记录。  * 所以把 8 条记录 + 头部 + 校验和拼成一个 212 字节的小结构，  * 每次擦扇区、整块写回，逻辑最简单、最不容易写坏。  *  * 为什么 index 0 是最新的一条：菜单里最常看的就是最近一次。  * ====================================================================== */ 
 #include "app_config.h"
#include "app_log.h"
#include "app_param.h"
#include "app_tick.h"

/* 整块结构一次读写, 必须 <= 1K(FLASH_DATA_BUFFER_SIZE 字) */
typedef struct
{
    uint32 magic;
    uint16 version;
    uint16 size;
    uint32 count;                                   /* 累计写入条数(不因覆盖而减少) */
    uint32 head;                                    /* 下一条要写的位置 0~APP_LOG_MAX-1 */
    app_log_entry_struct entry[APP_LOG_MAX];
    uint32 checksum;
} app_log_struct;

#define APP_LOG_WORDS               (sizeof(app_log_struct) / 4)

static app_log_struct g_log;

/*-------------------------------------------------------------------------------------------------------------------
 * 校验和: 除最后一个字段外全部累加
 *-----------------------------------------------------------------------------------------------------------------*/
static uint32 app_log_checksum(void)
{
    const uint32 *word = (const uint32 *)&g_log;
    uint32 sum = 0;
    uint8  i;

    for(i = 0; i < (APP_LOG_WORDS - 1); i ++)
    {
        sum += word[i];
    }
    return sum;
}

static uint32 app_log_entry_checksum(const app_log_entry_struct *e)
{
    const uint32 *word = (const uint32 *)e;
    uint32 sum = 0;
    uint8  i;

    for(i = 0; i < (sizeof(app_log_entry_struct) / 4) - 1; i ++)
    {
        sum += word[i];
    }
    return sum;
}

/*-------------------------------------------------------------------------------------------------------------------
 * 上电加载
 *-----------------------------------------------------------------------------------------------------------------*/
void app_log_init(void)
{
    uint32 i;

    if(0 == flash_check(APP_LOG_SECTOR, APP_LOG_PAGE))
    {
        printf("[LOG ] blank -> empty\r\n");
    }
    else
    {
        flash_read_page(APP_LOG_SECTOR, APP_LOG_PAGE, (uint32 *)&g_log, (uint16)APP_LOG_WORDS);

        if((APP_LOG_MAGIC == g_log.magic) &&
           (sizeof(app_log_struct) == g_log.size) &&
           (app_log_checksum() == g_log.checksum))
        {
            printf("[LOG ] loaded, %d records\r\n", (int)g_log.count);
            return;
        }
        printf("[LOG ] invalid -> empty\r\n");
    }

    /* 初始化成空表 */
    for(i = 0; i < sizeof(app_log_struct); i ++)
    {
        ((uint8 *)&g_log)[i] = 0;
    }
    g_log.magic   = APP_LOG_MAGIC;
    g_log.version = APP_LOG_VERSION;
    g_log.size    = (uint16)sizeof(app_log_struct);
    g_log.count   = 0;
    g_log.head    = 0;
}

/*-------------------------------------------------------------------------------------------------------------------
 * 追加一条记录（= 参数被保存时的快照）
 * 返回参数     0 = 成功, 1 = 失败
 * 备注信息     整页重写: 一次擦除 + 一次写入; 记录满了就覆盖最旧的一条
 *-----------------------------------------------------------------------------------------------------------------*/
uint8 app_log_append(void)
{
    app_log_entry_struct *e;
    uint8 ret;

    e = &g_log.entry[g_log.head];

    e->seq          = g_log.count + 1;
    e->tick_s       = app_tick_get() / 1000;
    e->led_blink_ms = g_param.led_blink_ms;
    e->buzzer_freq  = g_param.buzzer_freq;
    e->breath_step  = g_param.breath_step;
    e->checksum     = app_log_entry_checksum(e);

    g_log.count ++;
    g_log.head = (g_log.head + 1) % APP_LOG_MAX;
    g_log.magic    = APP_LOG_MAGIC;
    g_log.version  = APP_LOG_VERSION;
    g_log.size     = (uint16)sizeof(app_log_struct);
    g_log.checksum = app_log_checksum();

    printf("[LOG ] append #%d (blink=%d freq=%d step=%d)\r\n",
           (int)e->seq, (int)e->led_blink_ms, (int)e->buzzer_freq, (int)e->breath_step);

    ret = flash_write_page(APP_LOG_SECTOR, APP_LOG_PAGE, (const uint32 *)&g_log, (uint16)APP_LOG_WORDS);

    printf("[LOG ] %s, %d words\r\n", (ret) ? ("FAIL") : ("OK"), (int)APP_LOG_WORDS);

    return ret;
}

void app_log_clear(void)
{
    uint32 i;

    for(i = 0; i < sizeof(app_log_struct); i ++)
    {
        ((uint8 *)&g_log)[i] = 0;
    }
    g_log.magic    = APP_LOG_MAGIC;
    g_log.version  = APP_LOG_VERSION;
    g_log.size     = (uint16)sizeof(app_log_struct);
    g_log.head     = 0;
    g_log.count    = 0;
    g_log.checksum = app_log_checksum();

    flash_erase_sector(APP_LOG_SECTOR, APP_LOG_PAGE);
    printf("[LOG ] cleared\r\n");
}

uint32 app_log_count(void)
{
    return g_log.count;
}

/* index 0 = 最新的一条 */
const app_log_entry_struct * app_log_get(uint8 index)
{
    uint8 pos;

    if((0 == g_log.count) || (index >= APP_LOG_MAX) || (index >= g_log.count))
    {
        return NULL;
    }

    /* head 指向"下一条要写的位置", 所以最新一条在 head-1 */
    pos = (uint8)((g_log.head + APP_LOG_MAX - 1 - index) % APP_LOG_MAX);

    return &g_log.entry[pos];
}

void app_log_show(void)
{
    uint8 i;

    printf("---- parameter log (%d records) ----\r\n", (int)g_log.count);
    for(i = 0; i < APP_LOG_MAX; i ++)
    {
        const app_log_entry_struct *e = app_log_get(i);
        if(NULL == e)
        {
            break;
        }
        printf("  #%d  t=%ds  blink=%d  freq=%d  step=%d\r\n",
               (int)e->seq, (int)e->tick_s, (int)e->led_blink_ms, (int)e->buzzer_freq, (int)e->breath_step);
    }
    printf("------------------------------------\r\n");
}
