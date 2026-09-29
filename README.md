# CH32V307VCT6 MounRiver 工程模板（已内含逐飞开源库）

一个**克隆下来就能直接编译**的 CH32V307VCT6 裸机工程模板。

- **IDE**：MounRiver Studio 2（VS Code 内核，自带 RISC-V GCC 8.2.0 与 WCH OpenOCD）
- **芯片**：CH32V307VCT6（青稞 V4F，RV32IMAFC，256K Flash / 64K SRAM 标称）
- **已内含**：逐飞科技 CH32V307 开源库 **V3.7.2**（`zf_common` / `zf_driver` / `zf_device` / `zf_components`）
- **实测构建**：`text 22964 / data 264 / bss 2700`，**0 error / 0 warning**

---

## 快速开始

```cmd
git clone https://github.com/FZZZZZZZZZ19/CH32.git
```

1. 用 **MounRiver Studio 2** 打开 `CH32` 目录（`File → Open Folder`）
2. `Project → Clean`
3. `Project → Build`

**没有第 4 步。** 不需要装库、不需要建链接、不需要改任何配置。

> 若打开后 MounRiver 提示工程配置需要刷新，让它刷新即可 ——
> `.cproject` 里用的是 `${workspace_loc}` 相对路径，不含任何机器相关的绝对路径。

---

## 第一次上手

```c
/* User/main.c 已经写好的最小示例 */
#include "zf_common_headfile.h"

int main(void)
{
    clock_init(SYSTEM_CLOCK_144M);   // 144MHz
    debug_init();                    // 调试串口

    printf("\r\nready, system_clock = %d MHz\r\n", system_clock / 1000000);

    while (1) { }
}
```

**接线（WCH-LinkE）**

| WCH-LinkE | CH32V307 |
|---|---|
| 3V3 | 3V3 |
| GND | GND |
| SWDIO | **PA13** |
| SWCLK | **PA14** |

**看串口输出**

> ⚠️ 调试串口是 **UART3 的 PB10(TX) / PB11(RX) @ 115200**，**不是 PA9**。
> 把 USB-TTL 的 RXD 接到板子的 **PB10**。

**PA13 / PA14 是调试口**，程序里不要配成普通 GPIO，否则会连不上调试器。

---

## 目录结构

```
CH32\
├── User\                      应用层 —— 你的代码写在这里
│   ├── main.c                 入口
│   ├── zf_isr.c               逐飞中断服务函数（约 50 个 *_IRQHandler）
│   ├── ch32v30x_it.c          NMI / HardFault
│   ├── system_ch32v30x.c      系统时钟 / FLASH-RAM 分区
│   └── ch32v30x_conf.h        外设库配置头
├── Core\                      core_riscv.c/h —— CSR 访问层
├── Peripheral\                WCH 标准外设库（inc 27 + src 27）
├── Startup\                   startup_ch32v30x_D8C.S（D8 版已在 .cproject 中排除）
├── Debug\                     debug.c —— Delay_* 与 _sbrk
├── Ld\Link.ld                 链接脚本：256K FLASH / 32K RAM
├── zf_common\      (14 文件)  逐飞公共层：clock/debug/fifo/font/function/interrupt/typedef
├── zf_driver\      (32 文件)  逐飞外设驱动层
├── zf_device\      (58 文件)  逐飞器件驱动层（含 libzf_device_config.a）
├── zf_components\  ( 4 文件)  逐飞应用组件（逐飞助手）
├── LICENSES\                  第三方许可说明与 GPL-3.0 全文
├── .cproject .project .launch  MounRiver 工程定义
└── SEEKFREE_MOUNRIVER_PLAN.md 接入过程记录（含踩坑与回滚方式）
```

**所有驱动都已经参与编译** —— `Peripheral\src` 下 27 个 WCH 驱动、以及
`zf_driver` / `zf_device` 下全部逐飞驱动都在构建里，你**直接调用 API 即可**，
不需要改 `.cproject` 去"添加源文件"。

---

## 关键配置要点

### 编译选项：`-march=rv32imafcxw -mabi=ilp32f`

CH32V307 有单精度硬件浮点，而且逐飞预编译库 `zf_device\libzf_device_config.a`
是**单精度硬浮点 ABI**，所以必须是 `ilp32f`。**改成软浮点 `ilp32` 会链接失败**：

```
failed to merge target specific data of file ... libzf_device_config.a
```

对应 `.cproject` 里的 `target.isa.fp = single` 与 `target.abi.fp = single`。
语言标准是 `gnu11`。

### 时钟

`User\main.c` 里 `clock_init(SYSTEM_CLOCK_144M)`，可选
24 / 48 / 72 / 96 / 120 / 144 MHz（见 `zf_common_clock.h`）。

逐飞的延时、PWM 占空比、串口波特率换算都依赖全局变量 `system_clock`，
用 `clock_init()` 改频率它会自动同步 —— **不要绕过它直接改 RCC 寄存器**。

### 调试串口

由逐飞的 `zf_common_debug.c` 负责（`printf` → `_write`）。默认配置在
`zf_common_debug.h`：

```c
#define DEBUG_UART_INDEX     (UART_3)
#define DEBUG_UART_BAUDRATE  (115200)
#define DEBUG_UART_TX_PIN    (UART3_MAP0_TX_B10)   // PB10
#define DEBUG_UART_RX_PIN    (UART3_MAP0_RX_B11)   // PB11
#define DEBUG_UART_USE_INTERRUPT (1)
```

**要换成别的串口**，改这几个宏之后，还必须把 `User\zf_isr.c` 里对应的
接收中断处理（现在是 `USART3_IRQHandler` 调用 `debug_interrupr_handler()`）
搬到新串口的中断函数里。

### `_write` 与 `_sbrk`

- `_write`（printf 重定向）**由逐飞提供**，`Debug\debug.c` 里原来那份已删除
- `Debug\debug.c` 里的 **`_sbrk` 必须保留** —— 逐飞摄像头驱动
  （`zf_device_mt9v03x_dvp.c` / `scc8660_dvp.c`）会调用 `malloc`，
  而逐飞库自己不提供 `_sbrk`

### FLASH / RAM 分区

`Ld\Link.ld` 当前是 **256K FLASH / 32K RAM**（WCH EVT 的默认值）。

CH32V30x 支持多种切分：`192K+128K` / `224K+96K` / **`256K+64K`** / `288K+32K` / `128K+192K`。

> ⚠️ **改动 `Link.ld` 必须同步改 `User\system_ch32v30x.c` 里的分区设置**，
> 否则会静默踩内存。

> 当前 `bss` 只有 2.7KB，32K RAM 够用。但逐飞的**摄像头驱动**需要大块图像缓冲，
> 真要用摄像头前建议先确认芯片实际的 FLASH/RAM 配置，再决定是否切成 64K RAM。

### `FLASH_FLAG_PGERR`

逐飞 SDK 里有、本工程使用的 WCH EVT SDK 里没有，已在
`Peripheral\inc\ch32v30x_flash.h` 用 `#ifndef` 保护补上（`zf_driver_flash.c` 需要）。
未来 WCH SDK 若补上同名宏，`#ifndef` 保证不会冲突。

---

## 怎么写自己的代码

1. **加功能**：直接在 `User\main.c` 的 `while(1)` 里写，或新建 `User\xxx.c`
   （`User` 目录下的 `.c` 会自动纳入构建）
2. **用逐飞驱动**：`#include "zf_common_headfile.h"` 后直接调用，例如
   ```c
   gpio_init(P20_0, GPO, 0, GPO_PUSH_PULL);
   pit_ms_init(PIT_TIMG0, 10);            // 10ms 定时中断
   uart_init(UART_1, 115200, UART1_TX_A9, UART1_RX_A10);
   ```
   各驱动的用法、可用引脚枚举都在对应头文件的注释里
3. **加中断处理**：中断服务函数已集中在 `User\zf_isr.c`，在对应
   `xxx_IRQHandler` 里填逻辑（逐飞驱动的回调也在这里接）
4. **换主频**：只改 `main()` 里 `clock_init()` 的参数
5. **参考例程**：逐飞库自带 12 个例程（GPIO/UART/ADC/PWM/PIT/EXTI/编码器/
   FLASH/定时器/printf调试/中断优先级/USB-CDC），
   见 <https://gitee.com/seekfree/CHV307_Library> 的 `Example\` 目录

---

## 更新逐飞库到新版本

本仓库内嵌的是一份**副本**（这样克隆即可编译）。要升级：

```cmd
git clone --depth 1 https://gitee.com/seekfree/CHV307_Library.git %TEMP%\CHV307
```

然后把 `%TEMP%\CHV307\Seekfree_CH32V307VCT6_Opensource_Library\libraries\` 下的
`zf_common` / `zf_driver` / `zf_device` / `zf_components` 四个目录，
覆盖本工程的同名目录，并同步更新 `LICENSES\SeekFree-GPL-3.0.txt`。

> 升级后建议先 `Project → Clean` 再 Build，并留意逐飞是否新增了
> 需要补的 SDK 宏（参考 `SEEKFREE_MOUNRIVER_PLAN.md` 里 `FLASH_FLAG_PGERR` 那类问题）。

---

## 已知限制

| 项 | 状态 |
|---|---|
| 编译 | ✅ 已在 MounRiver Studio 2 实测通过（0 error / 0 warning） |
| 链接（含逐飞预编译库） | ✅ 无 ABI 冲突、无重复定义 |
| 中断向量表 | ✅ `.vector` 位于 0x4，入口 0x0 |
| **实际烧写 / 上板运行** | ❌ **未验证**（制作此模板时手上无硬件） |
| LTO | 未启用（MounRiver 默认构建不含 LTO） |
| 32K RAM | ⚠️ 够当前代码；摄像头图像缓冲需重新评估分区 |

---

## 许可

本仓库包含三个来源的代码，许可各不相同 —— 详见 [`LICENSES/README.md`](LICENSES/README.md)。

| 部分 | 来源 | 许可 |
|---|---|---|
| `User\` 与工程配置 | 本仓库作者 | — |
| `Core\` `Peripheral\` `Startup\` `Debug\` | WCH CH32V30x EVT SDK | **Apache-2.0**（每文件头部有 SPDX 声明） |
| `zf_common\` `zf_driver\` `zf_device\` `zf_components\` | 逐飞科技 CH32V307 开源库 V3.7.2 | **GPL-3.0**（全文见 [`LICENSES/SeekFree-GPL-3.0.txt`](LICENSES/SeekFree-GPL-3.0.txt)） |

> 逐飞部分为**原样复制、未作修改**，每个源文件都保留了逐飞的完整版权与许可声明。
> **分发包含这部分代码的固件或源码时必须遵守 GPL-3.0 条款。**

---

## 附：接入过程记录

`SEEKFREE_MOUNRIVER_PLAN.md` 记录了把逐飞库接入本工程的完整方案与执行清单，
以及实际踩过的四个坑：

1. Eclipse 的 `<locationURI>PROJECT_LOC/...` 链接文件夹**不被 MounRiver 识别**
2. 根 sourceEntry 未排除库目录会导致**递归全扫**（把逐飞自带的 `main.c`、`isr.c`、
   `Example\` 12 个例程全编进去，与工程自身重复定义）
3. 工程默认是**软浮点** `rv32imacxw/ilp32`，与逐飞硬浮点预编译库**ABI 不兼容**
4. SDK 版本差异导致缺 `FLASH_FLAG_PGERR`

同文件也写了回滚方式（`.seekfree-bak\`）。
