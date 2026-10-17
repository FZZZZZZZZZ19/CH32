/* ======================== 【为什么要有这个文件】 ========================  * 任务 10 要的是三级菜单这个结构，而不是三个页面。所以这里有三个概念：  * 节点表（每个页面有哪些项）、层级 level(1/2/3)、编辑页（对可改的项）。  *  * 为什么用数据表加状态而不是 switch 套 switch：加一个页面 = 在表里加一行；  * 嵌套 switch 则要改三处。答辩时菜单可扩展这句话要靠这个结构撑住。  *  * 为什么编辑页要副本-确认-写回：用户按 LEFT/RIGHT 时若直接改 g_param，  * 退出不保存就无法实现。副本让正在编辑的值和已生效的值分开，  * 按 KEY1 才写回并追加一条历史记录。  * ====================================================================== */ 
 #include "app_config.h"
#include "app_menu.h"
#include "app_ui.h"
#include "app_key.h"
#include "app_param.h"
#include "app_log.h"
#include "app_tick.h"
#include "app_led.h"
#include "app_buzzer.h"
#include "app.h"                    /* 第 8 次提交：蜂鸣器扫频 app_buzzer_sweep_toggle() */

/*-------------------------------------------------------------------------------------------------------------------
 * 菜单项类型
 *-----------------------------------------------------------------------------------------------------------------*/
typedef enum
{
    MENU_ITEM_SUBMENU = 0,          /* 进入下一级节点 */
    MENU_ITEM_VALUE,                /* 进入第三级: 参数编辑 */
    MENU_ITEM_ACTION,               /* 直接执行 */
    MENU_ITEM_BACK                  /* 返回上一级 */
} menu_item_type_enum;

struct menu_item_struct;

typedef struct menu_item_struct
{
    const char *name;
    uint8       type;
    uint8       target;             /* SUBMENU: 目标节点号 */
    uint32     *value;              /* VALUE: 指向参数 */
    int32       min;
    int32       max;
    int32       step;
    const char *unit;
    void      (*action)(void);
} menu_item_struct;

typedef struct
{
    const char            *title;
    const menu_item_struct *items;
    uint8                  count;
} menu_node_struct;

/*-------------------------------------------------------------------------------------------------------------------
 * 各动作
 *-----------------------------------------------------------------------------------------------------------------*/
static void act_led_chase(void)     { app_led_chase_toggle();         app_ui_msg("chase"); }
static void act_beep(void)          { app_buzzer_start(g_param.buzzer_freq, 100);  app_ui_msg("beep"); }
static void act_beep_1s(void)       { app_buzzer_start(g_param.buzzer_freq, 1000); app_ui_msg("beep 1s"); }
static void act_beep_sweep(void)    { app_buzzer_sweep_toggle();                   app_ui_msg("sweep"); }
static void act_save_all(void)      { app_param_save(); app_log_append();  app_ui_msg("saved+logged"); }
static void act_load(void)          { app_param_load(); app_param_apply(); app_ui_msg("loaded"); }
static void act_default(void)       { app_param_reset(); app_param_save(); app_log_append(); app_param_apply(); app_ui_msg("default"); }
static void act_screen_demo(void)   { app_ui_msg("screen ok"); }
static void act_log_show(void)      { app_log_show(); app_ui_msg("log -> uart"); }
static void act_log_clear(void)     { app_log_clear(); app_ui_msg("log cleared"); }

/*-------------------------------------------------------------------------------------------------------------------
 * 节点表
 *  0 = 主菜单(L1), 1..N = 功能页(L2), 8 = 按键调试页
 *-----------------------------------------------------------------------------------------------------------------*/
#define NODE_MAIN       (0)
#define NODE_LED        (1)
#define NODE_PWM        (2)
#define NODE_TIMER      (3)
#define NODE_SCREEN     (4)
#define NODE_PARAM      (5)
#define NODE_LOG        (6)
#define NODE_KEYDBG     (7)
#define NODE_ABOUT      (8)
#define NODE_COUNT      (9)

/* ---- L1 ---- */
static const menu_item_struct item_main[] =
{
    { "LED",        MENU_ITEM_SUBMENU, NODE_LED,    NULL, 0, 0, 0, "",  NULL },
    { "PWM/Buzzer", MENU_ITEM_SUBMENU, NODE_PWM,    NULL, 0, 0, 0, "",  NULL },
    { "Timer",      MENU_ITEM_SUBMENU, NODE_TIMER,  NULL, 0, 0, 0, "",  NULL },
    { "Screen",     MENU_ITEM_SUBMENU, NODE_SCREEN, NULL, 0, 0, 0, "",  NULL },
    { "Params",     MENU_ITEM_SUBMENU, NODE_PARAM,  NULL, 0, 0, 0, "",  NULL },
    { "Log",        MENU_ITEM_SUBMENU, NODE_LOG,    NULL, 0, 0, 0, "",  NULL },
    { "Key Debug",  MENU_ITEM_SUBMENU, NODE_KEYDBG, NULL, 0, 0, 0, "",  NULL },
    { "About",      MENU_ITEM_SUBMENU, NODE_ABOUT,  NULL, 0, 0, 0, "",  NULL },
};

/* ---- L2: LED ---- */
static const menu_item_struct item_led[] =
{
    { "Blink",        MENU_ITEM_VALUE,  0, &g_param.led_blink_ms,   50, 5000,  50, "ms", NULL },
    { "Breath Step",  MENU_ITEM_VALUE,  0, &g_param.breath_step,     1,  500,   1, "",   NULL },
    { "LED Chase",    MENU_ITEM_ACTION, 0, NULL, 0, 0, 0, "", act_led_chase },
    { "< Back",       MENU_ITEM_BACK,   0, NULL, 0, 0, 0, "", NULL },
};

/* ---- L2: PWM ---- */
static const menu_item_struct item_pwm[] =
{
    { "Beep Freq",  MENU_ITEM_VALUE,  0, &g_param.buzzer_freq, 200, 8000, 100, "Hz", NULL },
    { "Beep Now",   MENU_ITEM_ACTION, 0, NULL, 0, 0, 0, "", act_beep },
    { "Beep 1s",    MENU_ITEM_ACTION, 0, NULL, 0, 0, 0, "", act_beep_1s },
    { "Beep Sweep", MENU_ITEM_ACTION, 0, NULL, 0, 0, 0, "", act_beep_sweep },
    { "< Back",     MENU_ITEM_BACK,   0, NULL, 0, 0, 0, "", NULL },
};

/* ---- L2: Timer ---- */
static const menu_item_struct item_timer[] =
{
    { "Tick is 1ms", MENU_ITEM_ACTION, 0, NULL, 0, 0, 0, "", NULL },   /* 占位, 见下 */
    { "< Back",      MENU_ITEM_BACK,   0, NULL, 0, 0, 0, "", NULL },
};

/* ---- L2: Screen ---- */
static const menu_item_struct item_screen[] =
{
    { "Screen Test", MENU_ITEM_ACTION, 0, NULL, 0, 0, 0, "", act_screen_demo },
    { "< Back",      MENU_ITEM_BACK,   0, NULL, 0, 0, 0, "", NULL },
};

/* ---- L2: Params (记录参数的主入口) ---- */
static const menu_item_struct item_param[] =
{
    { "LED Blink",   MENU_ITEM_VALUE,  0, &g_param.led_blink_ms,   50, 5000,  50, "ms", NULL },
    { "Beep Freq",   MENU_ITEM_VALUE,  0, &g_param.buzzer_freq,   200, 8000, 100, "Hz", NULL },
    { "Breath Step", MENU_ITEM_VALUE,  0, &g_param.breath_step,     1,  500,   1, "",   NULL },
    { "Save+Log",    MENU_ITEM_ACTION, 0, NULL, 0, 0, 0, "", act_save_all },
    { "Load",        MENU_ITEM_ACTION, 0, NULL, 0, 0, 0, "", act_load },
    { "Factory Rst", MENU_ITEM_ACTION, 0, NULL, 0, 0, 0, "", act_default },
    { "< Back",      MENU_ITEM_BACK,   0, NULL, 0, 0, 0, "", NULL },
};

/* ---- L2: Log ---- */
static const menu_item_struct item_log[] =
{
    { "Show Log",  MENU_ITEM_ACTION, 0, NULL, 0, 0, 0, "", act_log_show },
    { "Clear Log", MENU_ITEM_ACTION, 0, NULL, 0, 0, 0, "", act_log_clear },
    { "< Back",    MENU_ITEM_BACK,   0, NULL, 0, 0, 0, "", NULL },
};

/* ---- L2: KeyDebug（一个空页, 进入后显示实时编码）---- */
static const menu_item_struct item_keydbg[] =
{
    { "< Back", MENU_ITEM_BACK, 0, NULL, 0, 0, 0, "", NULL },
};

/* ---- L2: About ---- */
static const menu_item_struct item_about[] =
{
    { "Back", MENU_ITEM_BACK, 0, NULL, 0, 0, 0, "", NULL },
};

static const menu_node_struct menu_nodes[NODE_COUNT] =
{
    { "MAIN",      item_main,    sizeof(item_main)    / sizeof(menu_item_struct) },
    { "LED",       item_led,     sizeof(item_led)     / sizeof(menu_item_struct) },
    { "PWM/Buzzer",item_pwm,     sizeof(item_pwm)     / sizeof(menu_item_struct) },
    { "TIMER",     item_timer,   sizeof(item_timer)   / sizeof(menu_item_struct) },
    { "SCREEN",    item_screen,  sizeof(item_screen)  / sizeof(menu_item_struct) },
    { "PARAMS",    item_param,   sizeof(item_param)   / sizeof(menu_item_struct) },
    { "LOG",       item_log,     sizeof(item_log)     / sizeof(menu_item_struct) },
    { "KEY DEBUG", item_keydbg,  sizeof(item_keydbg)  / sizeof(menu_item_struct) },
    { "ABOUT",     item_about,   sizeof(item_about)   / sizeof(menu_item_struct) },
};

/*-------------------------------------------------------------------------------------------------------------------
 * 导航状态（三级就靠这三个变量表达）
 *-----------------------------------------------------------------------------------------------------------------*/
static uint8  menu_level      = 1;               /* 1 / 2 / 3 */
static uint8  menu_node       = NODE_MAIN;       /* 当前 L1/L2 节点 */
static uint8  menu_sel        = 0;               /* 当前项 */
static uint8  menu_redraw     = 1;

/* 第三级: 编辑中的项 */
static const menu_item_struct *menu_edit_item = NULL;
static int32  menu_edit_value = 0;

/*-------------------------------------------------------------------------------------------------------------------
 * 画当前页
 *-----------------------------------------------------------------------------------------------------------------*/
static void menu_draw_list(void)
{
    const menu_node_struct *node = &menu_nodes[menu_node];
    char line[24];
    uint8 i;

    sprintf(line, "%d/3 %s", (int)menu_level, node->title);
    app_ui_list_begin(line);

    for(i = 0; i < node->count; i ++)
    {
        const menu_item_struct *it = &node->items[i];

        switch(it->type)
        {
            case MENU_ITEM_VALUE:
                sprintf(line, "%-9s%4d%s", it->name, (int)(*(it->value)), it->unit);
                break;

            case MENU_ITEM_SUBMENU:
                sprintf(line, "%s >", it->name);
                break;

            default:
                sprintf(line, "%s", it->name);
                break;
        }

        app_ui_list_row(i, line, (i == menu_sel));
    }

    app_ui_list_clear_rows(node->count);
    app_ui_list_hint(APP_UI_HINT_LIST);

    /* KeyDebug 节点: 直接叠加实时编码 */
    if(NODE_KEYDBG == menu_node)
    {
        uint8 code = app_key_get_raw_code();
        sprintf(line, "A2A1A0= %d%d%d   ", (code >> 2) & 1, (code >> 1) & 1, code & 1);
        app_ui_list_row(1, line, 0);
        sprintf(line, "code= %d         ", code);
        app_ui_list_row(2, line, 0);
        sprintf(line, "key = %s", app_key_name(app_key_get_pressed()));
        app_ui_list_row(3, line, 0);
        sprintf(line, "log = %d rec     ", (int)app_log_count());
        app_ui_list_row(4, line, 0);
        menu_redraw = 1;                    /* KeyDebug 页每轮都刷新 */
    }

    if(NODE_ABOUT == menu_node)
    {
        sprintf(line, "sysclk %d MHz", (int)(system_clock / 1000000));
        app_ui_list_row(1, line, 0);
        sprintf(line, "uptime %d s", (int)(app_tick_get() / 1000));
        app_ui_list_row(2, line, 0);
        sprintf(line, "log %d rec", (int)app_log_count());
        app_ui_list_row(3, line, 0);
        app_ui_list_row(4, "sw v1.0  build", 0);
        sprintf(line, "%.16s", __DATE__);
        app_ui_list_row(5, line, 0);
    }
}

/*-------------------------------------------------------------------------------------------------------------------
 * 第三级编辑页
 *-----------------------------------------------------------------------------------------------------------------*/
static void menu_draw_edit(void)
{
    char line[24];

    app_ui_list_begin("3/3 EDIT");

    sprintf(line, "%-16s", menu_edit_item->name);
    app_ui_list_row(1, line, 0);

    /* 值放中间, 用大字体的感觉: 8x16 就靠缩进 + 高亮 */
    sprintf(line, "   %d %s", (int)menu_edit_value, menu_edit_item->unit);
    app_ui_list_row(3, line, 1);

    sprintf(line, "range %d ~ %d", (int)menu_edit_item->min, (int)menu_edit_item->max);
    app_ui_list_row(5, line, 0);

    sprintf(line, "step %d", (int)menu_edit_item->step);
    app_ui_list_row(6, line, 0);

    app_ui_list_hint(APP_UI_HINT_EDIT);
}

static void menu_enter_edit(const menu_item_struct *item)
{
    menu_edit_item  = item;
    menu_edit_value = (int32)(*(item->value));      /* 改的是副本, 确认后才写回 */
    menu_level      = 3;
    menu_redraw     = 1;
}

static void menu_edit_commit(void)
{
    if(NULL == menu_edit_item)
    {
        return;
    }

    *(menu_edit_item->value) = (uint32)menu_edit_value;
    app_param_apply();                              /* 立刻生效 */
    app_ui_msg("applied");
    printf("[MENU] %s = %d\r\n", menu_edit_item->name, (int)menu_edit_value);

    menu_edit_item = NULL;
    menu_level     = 2;
    menu_redraw    = 1;
}

/*-------------------------------------------------------------------------------------------------------------------
 * 按键处理（三级的状态机）
 *-----------------------------------------------------------------------------------------------------------------*/
void app_menu_task(void)
{
    app_key_enum       key;
    app_key_event_enum ev;
    const menu_node_struct *node;
    const menu_item_struct *it;

    /* KeyDebug / About 页也要周期性刷新 */
    if((NODE_KEYDBG == menu_node) || (NODE_ABOUT == menu_node))
    {
        static uint32 dbg_tick = 0;
        if(app_tick_reached(&dbg_tick, 200))
        {
            menu_redraw = 1;
        }
    }

    ev = app_key_get_event(&key);
    if(APP_KEY_EVENT_NONE == ev)
    {
        if(menu_redraw)
        {
            menu_redraw = 0;
            (3 == menu_level) ? menu_draw_edit() : menu_draw_list();
        }
        return;
    }

    /* ============================ 第三级: 参数编辑 ============================ */
    if(3 == menu_level)
    {
        switch(key)
        {
            case APP_KEY_LEFT:
                menu_edit_value -= menu_edit_item->step;
                if(menu_edit_value < menu_edit_item->min) { menu_edit_value = menu_edit_item->min; }
                menu_redraw = 1;
                break;

            case APP_KEY_RIGHT:
                menu_edit_value += menu_edit_item->step;
                if(menu_edit_value > menu_edit_item->max) { menu_edit_value = menu_edit_item->max; }
                menu_redraw = 1;
                break;

            case APP_KEY_MID:                           /* 确定 */
                menu_edit_commit();
                break;

            case APP_KEY_KEY1:                          /* 确定 + 落盘 + 记录 */
                menu_edit_commit();
                app_param_save();
                app_log_append();
                app_ui_msg("saved+logged");
                break;

            case APP_KEY_KEY2:                          /* 取消 */
                menu_edit_item = NULL;
                menu_level     = 2;
                menu_redraw    = 1;
                app_ui_msg("cancel");
                break;

            default:
                break;
        }
        return;
    }

    /* ============================ 第一/二级: 列表 ============================ */
    switch(key)
    {
        case APP_KEY_UP:
            if((APP_KEY_EVENT_PRESS == ev) || (APP_KEY_EVENT_REPEAT == ev))
            {
                menu_sel = (0 == menu_sel) ? (menu_nodes[menu_node].count - 1) : (menu_sel - 1);
                menu_redraw = 1;
            }
            break;

        case APP_KEY_DOWN:
            if((APP_KEY_EVENT_PRESS == ev) || (APP_KEY_EVENT_REPEAT == ev))
            {
                menu_sel = (menu_sel + 1) % menu_nodes[menu_node].count;
                menu_redraw = 1;
            }
            break;

        case APP_KEY_MID:
            if(APP_KEY_EVENT_PRESS != ev)
            {
                break;
            }
            node = &menu_nodes[menu_node];
            it   = &node->items[menu_sel];

            if(MENU_ITEM_SUBMENU == it->type)
            {
                menu_node   = it->target;
                menu_sel    = 0;
                menu_level  = (NODE_MAIN == menu_node) ? 1 : 2;
                menu_redraw = 1;
            }
            else if(MENU_ITEM_VALUE == it->type)
            {
                menu_enter_edit(it);                    /* 进第三级 */
            }
            else if(MENU_ITEM_ACTION == it->type)
            {
                if(NULL != it->action) { it->action(); }
                menu_redraw = 1;
            }
            else /* MENU_ITEM_BACK */
            {
                menu_node   = NODE_MAIN;
                menu_sel    = 0;
                menu_level  = 1;
                menu_redraw = 1;
            }
            break;

        case APP_KEY_KEY2:                              /* 快捷返回 */
            if(APP_KEY_EVENT_PRESS == ev)
            {
                menu_node   = NODE_MAIN;
                menu_sel    = 0;
                menu_level  = 1;
                menu_redraw = 1;
            }
            break;

        case APP_KEY_KEY1:                              /* 快捷保存 + 记录 */
            if(APP_KEY_EVENT_PRESS == ev)
            {
                app_param_save();
                app_log_append();
                app_ui_msg("saved+logged");
                menu_redraw = 1;
            }
            break;

        default:
            break;
    }
}

void app_menu_init(void)
{
    menu_level  = 1;
    menu_node   = NODE_MAIN;
    menu_sel    = 0;
    menu_redraw = 1;
    printf("[MENU] %d nodes, root items=%d\r\n",
           NODE_COUNT, (int)menu_nodes[NODE_MAIN].count);
}
