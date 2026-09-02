> 文档编号：04（前序文档为01、02、03）

# 阶段1：STM32 USART2 DMA 与 WSL 通信验证

## 1. 本阶段目标

本阶段只验证下面这一条链路，不接入 ROS 2、不接入 micro-ROS，也不接相机：

```text
STM32F042 USART2（DMA） <-> CP210x（USB 转 3.3 V TTL） <-> Windows USB <-> WSL Python
```

验收条件是：WSL 测试脚本完成 1 次握手和连续 1000 次 `PING/PONG`，全程无超时、无错序、无乱码。

当前阶段使用易观察的 ASCII 测试协议；它仅用于确认硬件和 DMA 稳定，后续阶段会在同一 UART BSP 上增加正式的二进制帧协议和 ROS 2 桥接。

## 2. 固件实现结构

```text
BSP/UART/
├── bsp_uart_driver.h       USART2 DMA 底层接口
└── bsp_uart_driver.c       128 B RX DMA 循环缓冲、非阻塞读取、TX DMA

User_tasks/
├── uart_test_task.h        阶段1任务创建接口
└── uart_test_task.c        HELLO/PING 命令解析及 READY/PONG 响应
```

分层边界：

- `BSP/UART` 只认识 USART2、DMA 和 HAL，不包含测试协议、ROS 2 或业务状态。
- `User_tasks/uart_test_task.c` 只实现阶段1协议和 FreeRTOS 轮询任务。
- `main.c` 仍只负责外设初始化、任务集中创建和启动调度器。

阶段1 UART 任务参数：

- 任务栈：64 words，即 256 B。
- 优先级：`tskIDLE_PRIORITY + 2`。
- RX DMA 缓冲：128 B，Circular 模式。
- DMA 读取周期：2 ms。
- TX 超时：100 ms。
- USART2：115200 baud、8 data bits、no parity、1 stop bit、no flow control。

## 3. 阶段1测试协议

每条命令和响应均以换行结尾；MCU 同时接受 `LF` 和 `CRLF`。

| PC 发送 | MCU 返回 | 用途 |
| --- | --- | --- |
| `HELLO` | `READY,TACGLOVE_UART_DMA_STAGE1_R3,115200` | 确认修正版固件身份和波特率 |
| `PING,1` | `PONG,1` | 确认双向通信及序号一致 |
| 非法命令 | `ERR,BAD_COMMAND` | 确认解析器仍在运行 |
| 超长命令 | `ERR,LINE_TOO_LONG` | 防止命令缓冲区溢出 |

MCU上电后不会主动发送 `READY`。只有PC发送 `HELLO` 后MCU才返回READY，因此脚本显示握手成功就能证明MCU确实收到了本次HELLO，而不是读到了上电残留数据。

### R3针对“握手一次后PING超时”的修正

R2实机现象表明第一帧能够由DMA发出，但HAL的TX状态可能没有在完成中断中恢复，后续发送因此一直返回Busy。R3保留TX DMA，并增加硬件 `TC` 标志轮询兜底：最后一个停止位确实发出后，如果HAL仍处于 `BUSY_TX`，Driver调用 `HAL_UART_AbortTransmit()` 只结束TX并恢复READY，不会停止独立运行的RX循环DMA。

该修改属于 `BSP/UART` 代码层修复，本次没有增加新的CubeMX配置。CubeMX仍保持第5节同步清单中的设置。

## 4. 硬件接线

连接前确认 CP210x 的 UART 电平为 3.3 V TTL。

| STM32F042 | CP210x | 说明 |
| --- | --- | --- |
| PA2 / USART2_TX | RXD | TX 接对端 RX |
| PA3 / USART2_RX | TXD | RX 接对端 TX |
| GND | GND | 必须共地 |

注意：

- 不要把 TX 接 TX、RX 接 RX。
- 不要把 5 V TTL 信号直接接到 MCU 引脚。
- 如果板子已经由调试器或独立电源供电，不要再连接 CP210x 的 5 V 供电脚；本测试只需要 TXD、RXD、GND。

## 5. CubeMX 必须保持的配置

这些设置已经同时写入 `M0_Init.ioc` 和当前生成源码。以后用 CubeMX 重新生成代码时，请再次核对，防止通信被覆盖。

### 本阶段 CubeMX 同步清单

| CubeMX位置 | 正确值 | 本次修正原因 | 重新生成后检查 |
| --- | --- | --- | --- |
| USART2 → DMA Settings → USART2_RX | Circular / High | RX需要连续接收，不能单次停止 | `usart.c` 中RX为 `DMA_CIRCULAR` |
| USART2 → DMA Settings → USART2_TX | Normal / High | 每条响应是一次性发送；Circular会使HAL保持Busy | `usart.c` 中TX为 `DMA_NORMAL` |
| System Core → NVIC → DMA1 Channel4_5 | Enabled / Priority 3 | TX、RX DMA完成事件需要中断处理 | `dma.c` 中已Enable且优先级3 |
| System Core → NVIC → USART2 global interrupt | Enabled / Priority 3 | TX DMA结束后需要UART TC中断恢复READY | `stm32f0xx_it.c` 中存在 `USART2_IRQHandler()` |
| Project Manager → Code Generator → Delete previously generated files... | Unchecked | 防止自定义 `Middlewares/FreeRTOS` 被删除 | `.ioc` 中为 `ProjectManager.DeletePrevious=false` |

本次已经直接更新 `M0_Init.ioc`。你在CubeMX界面打开工程后仍应逐项确认；确认无误后再Generate Code，并按照最后一列复查生成源码。以后凡涉及CubeMX配置的改动，每次交付都会附同格式的同步清单。

### USART2 / Parameter Settings

- Mode：Asynchronous
- Baud Rate：115200 Bits/s
- Word Length：8 Bits
- Parity：None
- Stop Bits：1
- Hardware Flow Control：None
- TX：PA2
- RX：PA3

### USART2 / DMA Settings

| DMA Request | Channel | Direction | Mode | Priority |
| --- | --- | --- | --- | --- |
| USART2_RX | DMA1 Channel 5 | Peripheral to Memory | **Circular** | **High** |
| USART2_TX | DMA1 Channel 4 | Memory to Peripheral | Normal | **High** |

### NVIC Settings

- `DMA1 Channel 4 and 5 interrupt`：Enabled，Preemption Priority = 3。
- `USART2 global interrupt`：Enabled，Preemption Priority = 3。

USART2 全局中断不能省略。TX DMA 搬运完成后，HAL 仍需要 USART2 的 TC 中断结束本次发送并把 UART 状态恢复为 READY；如果只开 DMA 中断，常见现象是第一帧发出后后续发送全部超时。

### Project Manager / Advanced Settings

- 取消勾选 `Delete previously generated files when not re-generated`，即保持 `ProjectManager.DeletePrevious=false`。否则 CubeMX 可能清理自定义的 `Middlewares/FreeRTOS`，导致本机型 `FreeRTOSConfig.h` 和故障 Hook 丢失。

保持初始化顺序：

```text
MX_GPIO_Init()
MX_DMA_Init()
MX_USART2_UART_Init()
```

USART2 已被正式通信链路独占。不要再把 `printf`、`scanf` 或阻塞式 `HAL_UART_Transmit/Receive` 放到 USART2，否则会向协议中插入额外字符或与 DMA 竞争。

## 6. 构建和烧录

在 Windows PowerShell 中执行：

```powershell
cd D:\M0\five_data\qyan_Tacglove
cmake --preset Release
cmake --build --preset Release
```

已验证可生成：

```text
D:\M0\five_data\qyan_Tacglove\build\cmake\Release\M0_Init.elf
```

也可以烧录同目录下由工具链生成的 `.hex` 或 `.bin`（若你的烧录工具配置为使用对应格式）。烧录后复位板卡。

本次 Release 构建结果：

- Flash：14620 B / 32768 B，44.62%。
- 静态 RAM：4488 B / 6144 B，73.05%。
- FreeRTOS heap：2048 B，已经包含在上述静态 RAM 中；应用任务栈从该 heap 运行时分配。
- UART 测试任务栈：256 B；编译器静态分析显示任务入口自身最大静态栈帧为 56 B，预留了 RTOS 上下文和下层函数调用空间。

## 7. 将 CP210x 连接给 WSL 2

WSL 2 不能默认直接占用 Windows USB 设备，需要使用 `usbipd-win`。以下命令对应 usbipd-win 5.x；执行时保持一个 WSL 终端处于打开状态。

### 7.1 Windows 管理员 PowerShell：首次共享设备

```powershell
usbipd list
```

找到名称包含 `CP210x`、`Silicon Labs` 或 `CP210` 的设备，记下它的 `BUSID`，例如 `4-4`。然后执行：

```powershell
usbipd bind --busid 4-4
usbipd list
```

期望该设备状态变为 `Shared`。`bind` 需要管理员权限，并且共享设置通常可以跨重启保留。

如果系统没有 `usbipd` 命令，先按 Microsoft 的 WSL USB 文档安装 usbipd-win，并执行 `wsl --update`。

### 7.2 Windows 普通 PowerShell：附加到 WSL

```powershell
usbipd attach --wsl --busid 4-4
usbipd list
```

期望状态变为 `Attached`。USB 设备拔插或 WSL 重启后通常需要再次执行 `attach`；设备附加给 WSL 时，Windows 串口软件不能同时占用它。

### 7.3 WSL：确认串口节点

```bash
lsusb
dmesg | tail -n 30
ls -l /dev/ttyUSB*
```

期望 `lsusb` 能看到 Silicon Labs CP210x，并出现类似 `/dev/ttyUSB0` 的设备节点。实际编号可能是 `/dev/ttyUSB1`，以后续命令显示为准。

如果普通用户出现 `Permission denied`，请明确把日常开发用户 `embedded` 加入串口组。不要在 root 终端使用 `$USER`，因为那样只会把 root 加入组：

```bash
sudo usermod -aG dialout embedded
newgrp dialout
id
ls -l /dev/ttyUSB0
```

`id` 的输出必须包含 `dialout`。`newgrp dialout` 会开启一个已应用新组权限的子Shell；也可以退出所有 WSL 终端，在 Windows PowerShell 执行 `wsl --shutdown` 后重新打开并重新 `usbipd attach`。root 测试只能用于定位权限问题，后续 ROS 2 和串口桥应使用普通用户运行。

## 8. 安装依赖并执行测试

在 WSL Ubuntu 中安装 pyserial：

```bash
sudo apt update
sudo apt install python3-serial
```

开始 1000 次往返测试：

```bash
python3 /mnt/d/M0/five_data/AppEncrypt/uart_dma_stage1_test.py /dev/ttyUSB0 --count 1000
```

如果串口节点不是 `/dev/ttyUSB0`，请替换成实际节点。

期望看到类似输出：

```text
打开 /dev/ttyUSB0，115200 baud...
握手成功: READY,TACGLOVE_UART_DMA_STAGE1_R3,115200
进度: 100/1000
...
进度: 1000/1000

PASS: UART DMA阶段1测试通过
成功往返: 1000次
```

只有出现最后的 `PASS` 才表示阶段1通过。请把完整终端输出发回来，我们确认后再进入阶段2。

## 9. 未通过时如何定位

| 现象 | 优先检查 |
| --- | --- |
| `usbipd` 找不到命令 | Windows 尚未安装 usbipd-win，或安装后终端未重开 |
| WSL 中 `lsusb` 看不到 CP210x | Windows 侧未 `bind/attach`，BUSID 选错，或 WSL 不是版本2 |
| 有 `lsusb` 设备但没有 `/dev/ttyUSB*` | 查看 `dmesg`；更新 WSL 内核；确认 `cp210x` 驱动已加载 |
| `Permission denied` | 将用户加入 `dialout` 后重新登录 WSL |
| 握手超时 | PA2/PA3 是否交叉、是否共地、板卡是否复位运行、波特率是否115200 |
| 收到不带R3的旧READY | 板上仍是旧固件；烧录最新R3固件并复位 |
| 只能收到第一帧，随后超时 | TX DMA必须为Normal；USART2 global interrupt必须启用；检查是否被CubeMX重新生成覆盖 |
| 运行一段时间后超时 | USART2_RX DMA 是否为 Circular；DMA IRQ 是否启用；接线是否松动 |
| 收到乱码 | 两端波特率/8N1不一致、电平不是3.3 V TTL、地线质量差 |
| 响应中混入 `Hello World` 或其他文字 | USART2 上仍存在 `printf` 或其他阻塞式 UART 调试输出 |

## 10. 本阶段不做的内容

- 不把现有 `libmicroros.a` 链接进 STM32F042。该库是 ARMv7E-M 构建产物，与 Cortex-M0 的 ARMv6-M 不兼容。
- 不在 MCU 上创建 micro-ROS node、publisher 或 subscriber。
- 不启动 ROS 2 bridge。
- 不接入六路 USB 相机，也不改变现有 LED/按键业务状态机。

阶段1通过后，阶段2才会定义并实现有 CRC、序号、消息类型和长度字段的正式 MCU 串口帧，以及 WSL 端协议解析器。

## 11. 官方参考

- Microsoft：<https://learn.microsoft.com/windows/wsl/connect-usb>
- usbipd-win WSL support：<https://github.com/dorssel/usbipd-win/wiki/WSL-support>
