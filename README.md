# CH32V307VCT6 MounRiver 工程模板（已接入逐飞开源库）

一个可直接在 **MounRiver Studio 2** 里打开、编译的 CH32V307VCT6 裸机工程模板，
已经完成 **逐飞科技 CH32V307 开源库（V3.7.2）** 的接入。

- 构建系统：MounRiver Studio 2（VS Code 内核）+ 其自带的 RISC-V GCC 8.2.0 与 WCH OpenOCD
- 目标芯片：CH32V307VCT6（青稞 V4F，RV32IMAFC）
- 实测构建：`text 22964 / data 264 / bss 2700`，**0 error / 0 warning**

---

## 快速开始

```cmd
git clone https://github.com/FZZZZZZZZZ19/CH32.git
cd CH32
setup-seekfree.cmd
```

`setup-seekfree.cmd` 会做两件事（幂等，可重复跑）：

1. 若本地没有逐飞库 → 从 Gitee 浅克隆一份到 `E:\RISC-V\libraries\CHV307_Library`
   （可传参换位置：`setup-seekfree.cmd D:\libs\CHV307_Library`）
2. 在工程根建立 4 个目录联接（junction）：`zf_common` / `zf_driver` / `zf_device` / `zf_components`

然后用 **MounRiver Studio 2** 打开本工程目录 → `Project → Clean` → `Build`。

> ⚠️ 必须先跑 `setup-seekfree.cmd` 再编译。逐飞库不在仓库里（原因见下），
> 没建 junction 的话会报一堆 `zf_common_headfile.h: No such file or directory`。

---

## 为什么逐飞库不在仓库里

逐飞库是**第三方 GPL-3.0** 代码，且本工程希望"一份库、多个工程共用"。所以：

- 工程通过 **4 个 NTFS 目录联接**引用库，而不是把源码拷进来
- git **无法保存 junction**（会把它当普通目录递归展开，把 108 个文件全量提交，
  克隆后变成不再同步的"死副本"），因此 `.gitignore` 排除了这 4 个名字
- 克隆后用 `setup-seekfree.cmd` 重建，效果等价

升级逐飞库只需一处：

```cmd
cd E:\RISC-V\libraries\CHV307_Library
git pull
```

所有引用它的工程同时生效，**不需要改任何工程配置**。

---

## 目录结构

```
CH32V307VCT\
├── User\                      应用层（你的代码）
│   ├── main.c                 入口：clock_init(144M) + debug_init()
│   ├── zf_isr.c               逐飞中断服务函数（约 50 个 *_IRQHandler）
│   ├── ch32v30x_it.c          NMI / HardFault（WCH 硬件压栈版本）
│   ├── system_ch32v30x.c      系统时钟与 FLASH/RAM 分区
│   └── ch32v30x_conf.h        外设库配置头
├── Core\                      core_riscv.c/h（CSR 访问层）
├── Peripheral\                WCH 标准外设库（inc + src）
├── Startup\                   startup_ch32v30x_D8C.S（D8 版已排除）
├── Debug\                     debug.c/h（Delay_* 与 _sbrk；_write 已移除）
├── Ld\Link.ld                 链接脚本：256K FLASH / 32K RAM
├── zf_common\      ─┐
├── zf_driver\       │ 4 个 junction → 逐飞库（不在版本控制内）
├── zf_device\       │
├── zf_components\  ─┘
├── target\                    （如用到可放 OpenOCD 配置）
├── setup-seekfree.cmd         克隆库 + 建 junction
└── SEEKFREE_MOUNRIVER_PLAN.md 接入过程与踩坑记录（含回滚方式）
```

`zf_*` 各目录内容：

| 目录 | 内容 |
|---|---|
| `zf_common` | clock / debug / fifo / font / function / interrupt / typedef |
| `zf_driver` | adc / delay / dvp / encoder / exti / flash / gpio / iic / pit / pwm / soft_iic / soft_spi / spi / timer / uart / usb_cdc |
| `zf_device` | 摄像头(mt9v03x, scc8660)、IMU(icm20602/660ra/963ra, mpu6050)、屏幕(ips114/200, tft180, oled)、无线(无线串口, ch573, ch9141, wifi, ble6a20)、存储(w25q32, k24c02)、ToF(dl1a, dl1b, dm1xa)、GNSS、按键、AHT20、虚拟示波器 |
| `zf_components` | seekfree_assistant（逐飞助手上位机协议） |

---

## 必须知道的几个关键点

### 1. 调试串口是 UART3 的 PB10 / PB11，不是 PA9

`printf` 由逐飞层重定向（`zf_common_debug.c` 的 `_write`），默认：

```c
#define DEBUG_UART_INDEX   (UART_3)
#define DEBUG_UART_BAUDRATE (115200)
#define DEBUG_UART_TX_PIN  (UART3_MAP0_TX_B10)   // PB10
#define DEBUG_UART_RX_PIN  (UART3_MAP0_RX_B11)   // PB11
```

要改回 USART1(PA9)，需同时改 `zf_common_debug.h` 的宏 **和** `User\zf_isr.c` 里
接收中断的位置（`USART3_IRQHandler` 里的 `debug_interrupr_handler()`）。

### 2. 编译选项：`-march=rv32imafcxw -mabi=ilp32f`

CH32V307 有单精度硬件浮点，且逐飞预编译库 `zf_device\libzf_device_config.a`
是**单精度硬浮点 ABI**，所以必须是 `ilp32f`。若改成软浮点 `ilp32`，链接会报：

```
failed to merge target specific data of file ...libzf_device_config.a
```

对应 `.cproject` 里的 `target.isa.fp = single` 与 `target.abi.fp = single`。

### 3. `FLASH_FLAG_PGERR` 是本工程补的

逐飞 SDK 里有、WCH EVT SDK 里没有，已在 `Peripheral\inc\ch32v30x_flash.h`
用 `#ifndef` 保护补上（`zf_driver_flash.c` 需要）。

### 4. `_write` 归属

`Debug\debug.c` 里原本的 `_write`（USART1）**已被删除**，改用逐飞的（UART3）。
但 `Debug\debug.c` 的 **`_sbrk` 必须保留** —— 逐飞摄像头驱动
（`zf_device_mt9v03x_dvp.c` / `scc8660_dvp.c`）会调用 `malloc`，而逐飞库自己不提供 `_sbrk`。

### 5. FLASH / RAM 分区

`Ld\Link.ld` 当前是 **256K FLASH / 32K RAM**（WCH EVT 默认值）。
CH32V30x 支持多种切分（192+128 / 224+96 / **256+64** / 288+32 / 128+192），
**若改动 `Link.ld` 必须同步改 `system_ch32v30x.c` 里的分区设置，否则会静默踩内存。**

> 32K RAM 对逐飞**摄像头**的大块图像缓冲可能不够（当前 bss 仅 2.7KB）。
> 真要用摄像头前，建议先确认芯片实际的 FLASH/RAM 配置再决定是否切成 64K RAM。

### 6. 时钟

`User\main.c` 里 `clock_init(SYSTEM_CLOCK_144M)`，可选 24/48/72/96/120/144MHz
（见 `zf_common_clock.h`）。逐飞的延时 / PWM / 波特率换算依赖全局 `system_clock`，
改了主频它会自动同步。

### 7. 烧录接线

使用 **WCH-LinkE**：`3V3→3V3`、`GND→GND`、`SWDIO→PA13`、`SWCLK→PA14`。

**PA13/PA14 是调试口，程序里不要配成普通 GPIO**，否则会连不上调试器。

---

## 现状与限制

| 项 | 状态 |
|---|---|
| 编译 | ✅ 已在 MounRiver Studio 2 实测通过（0 error / 0 warning） |
| 链接（含逐飞预编译库） | ✅ 无 ABI 冲突、无重复定义 |
| **实际烧写 / 上板运行** | ❌ **未验证**（写此模板时无硬件） |
| LTO | 未启用（MounRiver 默认构建无 LTO） |
| 32K RAM | ⚠️ 够当前代码用；摄像头缓冲需评估 |

---

## 许可

- 本工程的应用层代码（`User\`）与工程配置：可自行决定
- `Core\` `Peripheral\` `Startup\` `Debug\`：WCH CH32V30x EVT SDK，**Apache-2.0**
- 逐飞 CH32V307 开源库（通过 junction 引用的 `zf_*`）：**GPL-3.0**
  - 来源 <https://gitee.com/seekfree/CHV307_Library>
  - 本仓库**不再分发**其源码，需用 `setup-seekfree.cmd` 自行获取
  - 若你分发包含逐飞库的固件，请遵守其 GPL-3.0 条款并保留其版权声明

---

## 接入过程记录

`SEEKFREE_MOUNRIVER_PLAN.md` 里记了完整的接入方案、执行清单、
以及踩过的坑（Eclipse 链接文件夹不被识别、根 sourceEntry 递归全扫、
ABI 是软浮点、`FLASH_FLAG_PGERR` 缺失）和回滚方式。
