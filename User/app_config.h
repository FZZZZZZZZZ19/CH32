/* ======================== 【为什么要有这个文件】 ========================  * 引脚是哪个、蜂鸣器用哪路 PWM、参数区在第几扇区、按键怎么编码 —— 这些是  * 「板子长什么样」的事实。写在各 .c 里的话，换板子或改一个引脚就要满工程找。  * 集中到这一个头文件：改板子只改这里，读代码也从这里开始。  *  * 附注：这些值要参与数组维度和位运算（如 APP_LED_PIN[APP_LED_NUM]），  * 所以只能用 #define —— C 里的 const 变量不是编译期常量。  * ====================================================================== */ 
 /********************************************************************************
* File Name          : app_config.h
* Description        : 本工程所有硬件相关的定义都集中在这里
*                      换板子/换引脚 只需要改这一个文件
********************************************************************************/
#ifndef _app_config_h_
#define _app_config_h_

#include "zf_common_headfile.h"                 /* 逐飞库总头文件，包含全部外设 API */

/* ============================ 版本 ============================ */
#define APP_VERSION_STR             "v0.5"

/* ============================ LED =============================
 * 板载可编程 LED 共 4 个（原理图丝印 LED5~LED8），全部是"共阴"接法：
 *   MCU 引脚 ──限流电阻──|>|── GND
 * 所以：高电平点亮、低电平熄灭（不要照抄别的板子的低电平点亮）。
 * 丝印 LED1~LED4 是电源指示（5V / 3.3V / 3.3V_MAIN / 3.3V_CAM），程序控制不了。
 * 原理图上还有一个网络 LIGHT4_C10(PC10)，但最小系统板上没有对应 LED，别用。
 */
#define APP_LED_NUM                 (4)
#define APP_LED_IDX_A11             (0)         /* 丝印 LED8  <- LIGHT1_A11  */
#define APP_LED_IDX_A12             (1)         /* 丝印 LED7  <- LIGHT2_A12  */
#define APP_LED_IDX_A15             (2)         /* 丝印 LED6  <- LIGHT3_A15  */
#define APP_LED_IDX_D1              (3)         /* 丝印 LED5  <- LIGHT5_D1   */
#define APP_LED_ON_LEVEL            (GPIO_HIGH)
#define APP_LED_OFF_LEVEL           (GPIO_LOW)

/* ========================== 无源蜂鸣器 ==========================
 * PA8 经 AO3400A 低边开关驱动，高电平导通；谐振频率 2.731 kHz。
 * PA8 = TIM1_CH1 的默认映射(MAP0)。
 */
#define APP_BUZZER_PWM              (TIM1_PWM_MAP0_CH1_A8)
#define APP_BUZZER_FREQ_DEFAULT     (2700)                      /* 2700 Hz 最响 */
#define APP_BUZZER_DUTY_ON          (PWM_DUTY_MAX / 2)          /* 50% 方波      */
#define APP_BUZZER_DUTY_OFF         (0)                         /* 0% = 不响     */

/* ======================= 呼吸灯（PWM 演示） =======================
 * PA11 既是 LED8 又是 TIM1_CH4 的 MAP0 引脚。
 * 与蜂鸣器同属 TIM1/MAP0，因此可以同时使用（共享频率，占空比各自独立）。
 * 注意：一旦用于 PWM，这一路 LED 就不要再当普通 GPIO 用了。
 */
#define APP_BREATH_PWM              (TIM1_PWM_MAP0_CH4_A11)
#define APP_BREATH_STEP             (100)                       /* 每圈变化 100/10000 */

/* ========================== 系统节拍 ==========================
 * TIM6 每 1 ms 中断一次，维护全局毫秒计数 g_app_tick_ms。
 * 中断体写在 User/zf_isr.c 的 TIM6_IRQHandler 里。
 * 约束：TIM6 从此被节拍独占，不能再用于 PWM / 其它 PIT。
 */
#define APP_TICK_MS_PERIOD          (1)

/* LED 二进制计数显示用哪几路（第 0 路 A11 归 PWM 呼吸灯，不参与）*/
#define APP_LED_BIN_FIRST           (APP_LED_IDX_A12)
#define APP_LED_BIN_NUM             (3)

/* ========================= 1.8 寸 TFT 屏 (SPI2) =========================
 * 硬件接线（与库默认宏一致，无需改库）：
 *   SCK = PB13 (SPI2_MAP0)     MOSI = PB15 (SPI2_MAP0)
 *   RES = PB7                  DC   = PD7
 *   CS  = PD4                  BL   = PD0
 */
#define APP_DISPLAY_DIR             (TFT180_PORTAIT)        /* 竖屏: 宽128 x 高160 */
#define APP_DISPLAY_FONT            (TFT180_8X16_FONT)      /* 8x16 字体, 一行 16 字符 */
#define APP_DISPLAY_PEN             (RGB565_BLACK)
#define APP_DISPLAY_BG              (RGB565_WHITE)
#define APP_DISPLAY_REFRESH_MS      (100)                   /* 屏幕刷新周期 10 Hz */

/* ========================= FLASH 参数保存区 =========================
 * ⚠ CH32V307 的擦除单位是 4KB（整个扇区），所以 sector 63 这一个扇区
 *   只能放一份数据，page 只是库内部把 4K 切块的写法。
 * 扇区 63 = 0x0803F000 = 252KB 处，本工程程序约 23KB，绝不会撞车。
 */
#define APP_FLASH_SECTOR            (63)
#define APP_FLASH_PAGE              (0)

#define APP_PARAM_MAGIC             (0x4F4B4348)        /* "CH32" 小端 */
#define APP_PARAM_VERSION           (0x0100)            /* v1.0 */

/* ==================== 按键：74HC148 三线编码 ====================
 *  code = (KEY_IN3 << 2) | (KEY_IN2 << 1) | KEY_IN1
 *  0=KEY2  1=M  2=R  3=KEY1  4=U  5=D  6=L  7=无键
 *  ⚠ 编码器输出是推挽的，外部已有 10k 上拉，这里再用内部上拉是"双保险"，无害
 */
#define APP_KEY_PIN_A0              (D8)        /* KEY_IN1_D8  */
#define APP_KEY_PIN_A1              (B12)       /* KEY_IN2_B12 */
#define APP_KEY_PIN_A2              (B14)       /* KEY_IN3_B14 = MSB */

#define APP_KEY_SCAN_MS             (10)        /* 采样周期 */
#define APP_KEY_DEBOUNCE_MS         (20)        /* 消抖时间：连续 20ms 稳定才算数 */
#define APP_KEY_LONG_MS             (700)       /* 长按判定 */
#define APP_KEY_REPEAT_MS           (120)       /* 长按后的连发周期 */

/* ==================== 参数记录（历史记录环）====================
 * 和参数区(扇区 63)分开: 记录区放扇区 62, 互不影响
 * 一个扇区 4KB 一次擦除, 整页(<=1KB)重写 = 一次擦写
 */
#define APP_LOG_SECTOR              (62)
#define APP_LOG_PAGE                (0)
#define APP_LOG_MAX                 (8)         /* 最多保留 8 条记录 */
#define APP_LOG_MAGIC               (0x474F4C48)    /* "HLOG" 小端 */
#define APP_LOG_VERSION             (0x0100)

#endif
