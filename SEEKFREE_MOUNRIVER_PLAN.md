# 逐飞库接入 MounRiver 工程 CH32V307VCT —— 实施方案

> ### ⚠️ 第一次实际构建失败的原因与修法（已修）
>
> MounRiver 首次构建报了一堆 `zf_common_headfile.h: No such file or directory`：
> 1. **Eclipse 链接文件夹没生效** —— `<locationURI>PROJECT_LOC/...</locationURI>` 这种写法
>    MounRiver 不认，工程根下没生成链接文件夹，于是 `${workspace_loc:/${ProjName}/zf_common}`
>    解析不到 → **include 被整条丢弃**。
> 2. **根 sourceEntry 没排除 `libraries`** —— junction 被当普通目录**递归全扫**，把逐飞自带的
>    `project/user/src/main.c`、`isr.c`、`Example/` 里 12 个例程与 12 份重复 libraries、
>    以及我自己的 `.seekfree-bak/` 全编进去了（会与你的 main / ISR 重复定义）。
>
> **修法**：放弃链接文件夹，改为在**工程根**建 4 个真实 junction（名字与逐飞自带工程一致），
> 根 entry 排除 `zf_common|zf_driver|zf_device|zf_components|.seekfree-bak`，
> include 与 sourceEntries 全部用**浅路径**。现在 `.cproject` 结构与逐飞自带的可用工程**完全一致**。
>
> 顺带确认：从生成的 makefile 看到 `-march=rv32imafcxw -mabi=ilp32f` 与 `-std=gnu11` **确实已生效**。

> ## ✅ 执行状态：已完成（2026-09-29）
>
> ### 实际执行的改动
> | # | 内容 |
> |---|---|
> | S1 | ~~建 junction `libraries\CHV307_Library`~~ → **已替换**：在工程根建 4 个 junction `zf_common`/`zf_driver`/`zf_device`/`zf_components` → `E:\RISC-V\libraries\CHV307_Library\Seekfree_...\libraries\<同名>` |
> | S2 | 复制 `ch32v30x_usbotg_device.{c,h}` 到 `Peripheral\{src,inc}`（解决硬阻塞） |
> | S3 | ~~`.project` 新增 `<linkedResources>`，4 个 `<link>`（`PROJECT_LOC` 相对 URI）~~ → **失败，已回退**：MounRiver 不认这种 URI（工程根下没有生成链接文件夹），导致 include 解析失败。**改为在工程根建 4 个真实 junction**（`zf_common`/`zf_driver`/`zf_device`/`zf_components` → E 盘库），`.project` 的 `linkedResources` 已移除。 |
> | S4 | `.cproject`：根 entry `excluding` 追加 `zf_common\|zf_driver\|zf_device\|zf_components\|.seekfree-bak`、4 条浅层 sourceEntries、4 条浅层 include、`-lzf_device_config`、`zf_device` 库搜索路径 |
> | S5 | `Debug\debug.c` **删除 `_write`**，保留 `_sbrk` / `Delay_*` |
> | S6 | 新增 `User\zf_isr.c`（逐飞 `isr.c` 去掉 NMI/HardFault 两个函数） |
> | S7 | 重写 `User\main.c`（`clock_init(SYSTEM_CLOCK_144M)` + `debug_init()`） |
> | S8 | `.cproject` 的 `c.compiler.std` 由 `gnu99` 改为 **`gnu11`**（对齐逐飞） |
>
> ### ⚠️ 执行中发现的两个计划外问题（必须知悉）
>
> **1. ABI 必须改（原计划判断错误）**
> 计划 §2 事实 1 写的是"ABI 无需改动"，**这是基于我误读了逐飞 `.cproject` 的选项**。实测：
> 工程实际构建参数是 `-march=rv32imacxw -mabi=ilp32`（软浮点，ELF 头 `soft-float ABI`），
> 与逐飞单精度硬浮点库冲突（`failed to merge target specific data`）。
> **已改**：在 `.cproject` 的 toolchain 级补入
> `target.isa.fp = isa.fp.single` 与 `target.abi.fp = abi.fp.single`
> → 预期生成 `-march=rv32imafcxw -mabi=ilp32f`。实测该组合链接干净（`failed to merge` = 0）。
>
> **2. SDK 版本差异：缺 `FLASH_FLAG_PGERR`**
> `zf_driver_flash.c:108` 用了 `FLASH_FLAG_PGERR`，逐飞 SDK 有（`0x04`）而本工程 EVT SDK 没有。
> **已改**：在 `Peripheral\inc\ch32v30x_flash.h` 用 `#ifndef` 保护补入
> `FLASH_FLAG_PGERR` / `FLASH_FLAG_BANK1_PGERR`。
> 另注：`FLASH_FLAG_OPTERR` 两边值不同（`0x01` vs `0x80000001`），逐飞未用到，暂不处理。
>
> ### ✅ MounRiver 实际构建通过（2026-09-29 15:55）
> ```
> riscv-none-embed-size --format=berkeley "CH32V307VCT.elf"
>    text    data     bss     dec     hex   filename
>   22964     264    2700   25928    6548   CH32V307VCT.elf
> Generated CH32V307VCT.hex
> 编译完成 (耗时 3s299ms)     ← 0 error / 0 warning
> ```
> 实证核对：
> - `obj\zf_common|zf_driver|zf_device|zf_components\subdir.mk` 与 `.o` 均生成（6+16+28+2 = **52** 个 zf 源文件）
> - 编译命令 `-I` 已含全部 4 条 `zf_*` 路径
> - **`_write` 来自 `./zf_common/zf_common_debug.o`**（不是 `Debug/debug.o`）→ S5 改动正确、无重复定义
> - `_sbrk` 仍在镜像中（`Debug\debug.c` 提供）→ 摄像头 `malloc` 可用
> - 镜像含 `_start` / `main` / `clock_init` / `debug_init` / `debug_interrupr_handler` / `uart_write_string`
>
> **仍未做**：实际烧写与上板运行（无硬件）。

> ### 验证结果（用 MounRiver 自带 GCC 8.2.0 全量实跑）
> ```
> 86 个对象（85 源文件 + startup_ch32v30x_D8C.S）
> 编译: 0 error / 0 warning
> 链接: exit=0，failed to merge = 0，multiple definition = 0
> 镜像: text 23224 / data 264 / bss 2700
> 向量表: .init @0x0, .vector @0x4   ✓
> _start / main / clock_init / debug_init / _sbrk / _write 均在镜像
> ```
> **仍未验证**：MounRiver GUI 里点的 Build、以及实际烧写上板（无硬件）。
>
> ### 回滚
> 原始文件备份在 `.seekfree-bak\`（`.project`/`.cproject`/`Debug\debug.c`/`User\main.c`）。
> `Peripheral\inc\ch32v30x_flash.h` 的改动是**纯追加**（`#ifndef` 保护），删掉那 8 行即还原。
> junction 用 `rmdir` 删除。

---

# 逐飞库接入 MounRiver 工程 CH32V307VCT —— 实施方案（原始，未修订）

> 目标：把 `E:\RISC-V\libraries\CHV307_Library`（V3.7.2）接入
> `C:\Users\lzega\mounriver-studio-projects\CH32V307VCT`，
> **保留你工程自己的结构**（`User\` + 自有 `Peripheral\`/`Core\`/`Startup\`）。
>
> 状态：**待批准执行**

---

## 0. 更省事的备选（先看一眼再决定）

逐飞库**自带一个完整可用的 MounRiver 工程**：
`E:\RISC-V\libraries\CHV307_Library\Seekfree_CH32V307VCT6_Opensource_Library\project\mrs\`
（含 `.cproject` / `.project` / `.wvproj` / `.launch`，链接文件夹、include、`-lzf_device_config`、Link.ld 全部配好）

**如果接受用逐飞自己的工程当开发主体，本节以下全部不需要做**——直接在 MounRiver 里打开它即可。

下面写的是"把逐飞库嫁接进你现有工程"的方案（你选的 A）。

---

## 1. 已核实的事实（全部本次实测）

| # | 事实 | 证据 |
|---|---|---|
| 1 | 逐飞 `zf_common_debug.c` **不能**从构建中移除 | `zf_assert`/`zf_log` 遍布 `zf_driver`/`zf_device`/`zf_components`/`zf_common` 共 30+ 文件；`seekfree_assistant_interface.c:94-95` 取 `debug_send_buffer`/`debug_read_ring_buffer` 函数指针 |
| 2 | 逐飞库**没有 `_sbrk`**，但摄像头驱动真的用 `malloc` | 全库搜 `_sbrk` 无定义；`zf_device_mt9v03x_dvp.c:330`、`zf_device_scc8660_dvp.c:311` 有 "FIFO buffer malloc error" |
| 3 | 你的 `Debug\debug.c` 提供 `_write`(L174, 带 `used`) 与 `_sbrk`(L239) | 读文件 |
| 4 | 你 `Peripheral\inc\ch32v30x.h:6638` include `ch32v30x_conf.h` | 读文件 |
| 5 | 你 `User\ch32v30x_conf.h` **没有** include `ch32v30x_usbotg_device.h` | 读文件（22 个外设 + it + misc） |
| 6 | 逐飞 `.project` 用 `PARENT-n-PROJECT_LOC/...` 相对 URI 做链接文件夹 | 读原始 XML |
| 7 | 逐飞 `.cproject`：include 11 条、libs = `m`+`zf_device_config`、linker paths = `../`+`zf_device`、script = 其 `sdk/Ld/Link.ld` | XML DOM |
| 8 | 逐飞 arch/abi = `rv32imafc` / `ilp32f` | `isa.base=rv32i` + `isa.fp=single` + `mul/atomic/compressed=true` + `abi.fp=single` |
| 9 | 逐飞 `defs` 为空 → **无需任何宏** | XML DOM `IS_VALUE_EMPTY` |
| 10 | 你工程 `Startup\startup_ch32v30x_D8.S` 已被正确排除，只编 D8C | `.cproject` `excluding="startup_ch32v30x_D8.S"` |
| 11 | 你工程可正常构建：`text 7256 / data 152 / bss 2068` | `riscv-none-embed-size.exe` |
| 12 | 你的主频宏是 `SYSCLK_FREQ_96MHz_HSE` | `User\system_ch32v30x.c:24` |
| 13 | 你的 `Ld\Link.ld` 是 288K FLASH / 32K RAM | `Ld\Link.ld:28-29` |
| 14 | `system_clock` 被 7 个 zf 文件依赖 | delay/pwm/timer/spi/flash/adc + clock |
| 15 | MounRiver 的 GCC 8.2.0 **原生接受** `interrupt("WCH-Interrupt-fast")`，零警告 | 实测编译 exit=0 |
| 16 | MounRiver 的 GCC 8.2.0 **拒绝** `_zicsr_zifencei` 后缀 | 实测 `error: unsupported ISA substring` |
| 17 | MounRiver 捆绑 make / WCH OpenOCD(`wlink`+`wch_riscv`) / 完整工具链 | 路径与二进制字符串实测 |

---

## 2. 四个冲突与决议

### A. `_write` 重复定义 —— 保留逐飞的，改你自己的

- 逐飞 `zf_common_debug.c:283` 定义 `_write`（→ UART3 PB10）；你 `Debug\debug.c:174` 也定义（→ USART1 PA9）。
- **逐飞的 debug 层不能砍**（事实 1），所以**编辑 `Debug\debug.c`：删掉它的 `_write` 函数，保留 `_sbrk` 与 `Delay_*`**。
- ⚠️ **后果：调试串口变成 UART3 的 PB10(TX)/PB11(RX) @115200**。改 `zf_common_debug.h:45-48` 可换回，但换回要同步改 `User\zf_isr.c` 里的接收中断位置。

### B. 中断函数重复 —— 拆分

- 你 `User\ch32v30x_it.c` 定义了 `NMI_Handler` / `HardFault_Handler`；
  逐飞 `project\user\src\isr.c` 也定义这两个，**另加约 40 个驱动 ISR**（`uart_isr_handler`/`pit_isr_handler`/`exti_isr_handler`/`tim_isr_handler`/`debug_interrupr_handler` 等，是逐飞驱动必需的钩子）。
- **决议**：新增 `User\zf_isr.c` = 逐飞 `isr.c` **删掉 `NMI_Handler` 与 `HardFault_Handler` 两个函数**；
  你的 `ch32v30x_it.c` 保留这两个（`WCH-Interrupt-fast` 版本，事实 15 说明 MounRiver GCC 支持）。

### C. 时钟归属 —— 让逐飞置位 `system_clock`

- 你的 `SystemInit()`（启动文件调用）按 `SYSCLK_FREQ_96MHz_HSE` 配到 **96MHz**；
  逐飞的 `zf_common_clock.c` 定义 `system_clock` 初值 **144M**，且 7 个 zf 文件用它算延时/PWM/波特率。
- **若不一致，延时会差 1.5 倍**。
- **决议**：`main()` 里调 `clock_init(SYSTEM_CLOCK_xxx)`，以它为准；`SystemInit()` 保留作为前置初始化。
- **主频取值见 §5 待定项 1。**

### D. USB OTG 头文件缺失（硬阻塞）—— 补齐两个文件

- 事实 4+5：你的 `ch32v30x.h` → `ch32v30x_conf.h` 链里没有 usbotg_device；
  而逐飞 `zf_common_headfile.h:100` 无条件 include `zf_driver_usb_cdc.h` → `ch32v30x_usbotg_device.h`。
- **决议**：从
  `E:\RISC-V\libraries\CHV307_Library\Seekfree_CH32V307VCT6_Opensource_Library\libraries\sdk\Peripheral\`
  复制 `ch32v30x_usbotg_device.c` → 工程 `Peripheral\src\`，`ch32v30x_usbotg_device.h` → 工程 `Peripheral\inc\`。
- 这是唯一的"SDK 版本拼接点"（逐飞 SDK 基于较早的 WCH SDK）。**编译后必须确认无警告**。

---

## 3. 目录链接方案

工程在 `C:\...`，库在 `E:\...`，**跨盘无法用相对 URI**，因此：

1. 建 junction：`C:\Users\lzega\mounriver-studio-projects\CH32V307VCT\libraries\CHV307_Library`
   → `E:\RISC-V\libraries\CHV307_Library`
   （`mklink /J`，普通权限即可，且已在别处验证过可用）
2. `.project` 的 `<linkedResources>` 加 4 个 link，用**相对** URI 避开跨盘问题：
   ```xml
   <link><name>zf_common</name><type>2</type>
     <locationURI>PROJECT_LOC/libraries/CHV307_Library/Seekfree_CH32V307VCT6_Opensource_Library/libraries/zf_common</locationURI></link>
   ```
   （`zf_driver` / `zf_device` / `zf_components` 同理）
3. `.cproject` 的根 `sourceEntries` 的 `excluding` **追加** `zf_common|zf_driver|zf_device|zf_components`，
   并为这 4 个名字各加一条 `<entry ... name="zf_xxx"/>`。

---

## 4. 改动清单

| 片 | 文件 | 改动 |
|---|---|---|
| S1 | 工程内 `libraries\CHV307_Library` | 新建 junction → E 盘库 |
| S2 | `Peripheral\{inc,src}\ch32v30x_usbotg_device.{h,c}` | 从逐飞 SDK 复制（解决硬阻塞 D） |
| S3 | `.project` | 加 4 个 `<link>` |
| S4 | `.cproject` | 4 条 sourceEntries + `excluding` 追加 + include 加 4 条 + libs 加 `zf_device_config` + linker paths 加 `${workspace_loc:/${ProjName}/zf_device}` |
| S5 | `Debug\debug.c` | **删掉 `_write`**（保留 `_sbrk`、`Delay_*`） |
| S6 | `User\zf_isr.c` | 新增：逐飞 `isr.c` 去掉 `NMI_Handler`/`HardFault_Handler` |
| S7 | `User\main.c` | 改为 `clock_init(...)` + `debug_init()` 的逐飞时序 |
| S8 | `.cproject` | 确认 `-march` 为 `rv32imafc`（**不要** `_zicsr_zifencei`，事实 16） |

**不改动**：`Ld\Link.ld`（见 §5 待定项 2）、`Startup\`、`Core\`、`Peripheral\` 其余文件、逐飞库本身（只读引用）。

---

## 5. 待你定的两件事

1. **主频**：`clock_init(SYSTEM_CLOCK_144M)` 还是保持现在的 96MHz？
   （逐飞库默认按 144M 写初值；你 blink 工程之前用 144M）
2. **`Ld\Link.ld`**：保持 288K/32K，还是改回 **256K/64K**？
   - 现在不改也能跑（代码才 7.4KB），但 **32K RAM 对逐飞摄像头缓冲可能不够**
   - 改之前应先确认芯片选项字节里实际的 FLASH/RAM 分区，否则与真实 SRAM 不符

---

## 6. 前置条件与验证

### 动手前必须做
- **关闭 MounRiver Studio**（否则它退出时会用内存模型覆盖 `.project`/`.cproject`）
- 备份：`.project`、`.cproject`、`Debug\debug.c`、`User\main.c`

### 验证步骤
1. MounRiver → Build → **0 error**（记录 warning 并逐条判断）
2. 检查 `obj\CH32V307VCT.map` 含：`clock_init`、`system_delay_ms`、`soft_iic_sccb_write_register`
   （证明 `libzf_device_config.a` 与 soft_iic/delay 正确链入）
3. `riscv-none-embed-size.exe obj\CH32V307VCT.elf` → FLASH < 256K、RAM < 32K(或 64K)
4. 串口接 **PB10**，确认打印 `system_clock` 与预期一致
5. 若改动 `Link.ld` → 用 WCHISPTool 核对芯片实际分区

### 回滚
改动前备份的 4 个文件恢复即可；junction 用 `rmdir` 删除。

---

## 7. 风险

| 风险 | 处置 |
|---|---|
| MounRiver 覆盖我的 XML 编辑 | 必须先关 IDE；改完先在 MounRiver 里 Refresh 再 Build |
| Eclipse 跨盘 linkedResource 不被识别 | 已用 junction + `PROJECT_LOC` 相对 URI 规避 |
| `ch32v30x_usbotg_device.h` 与你 EVT SDK 版本不兼容 | 编译期暴露；失败则退回"不启用 USB CDC"（需注释逐飞 headfile，不推荐） |
| `_write` 删改后 printf 静默无输出 | 用 PB10 实测确认 |
| 摄像头 `malloc` 因 `_sbrk` 缺失而失败 | 已保留 `Debug\debug.c` 的 `_sbrk` |

---

## Plan Verification

- **Mode**: fact-check (fallback — logicprobe plugin not installed)
- **Claims checked**: 17 confirmed, 0 refuted, 2 unverified（已显式标注为"待确认/回退方案"，非事实断言）
  - 未验证 1：`ch32v30x_usbotg_device.h` 与 EVT SDK 的版本兼容性 → 标注"编译后确认"
  - 未验证 2：Eclipse 对 `PROJECT_LOC/...` 形式 `locationURI` 的接受度 → 已用 junction 降低风险，仍标注为风险项
- **Escalation**: model verification NOT run（requires logicprobe plugin）。
  本方案无状态机 / ACK-重试 / 并发锁序，无需模型验证的 "always/never" 行为断言。
