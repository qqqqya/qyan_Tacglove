# 五指相机采集板固件开发流程

## 1. 已确认的硬件边界

- MCU：STM32F042K6T6，Cortex-M0，32 KB Flash，6 KB SRAM，当前系统时钟 48 MHz。
- USB 拓扑：CH338F 提供 1 路上行和 7 路下行；板上使用摄像头 1~5 与 1 路鱼眼，共 6 路 USB 下行。
- MCU 没有连接 CH338F 的 `RESET#`、`PWREN#`、`OVCUR#` 或 SMBus，因此当前固件不能枚举相机、搬运视频，也不能复位或读取 Hub 状态。视频枚举与采集由上位机通过 CH338F 完成。
- U5 为摄像头 1~5 提供公共 `5V5灯`，U6 为鱼眼提供独立 5V；两路 EN 均为硬件常开。当前硬件不能由 MCU 对相机逐路上下电。
- 指示灯为 7 颗串联 SK6805-EC15，不是 5 个独立 GPIO。设计意图应为：
  `PA4(RGB_Ctrl) -> 74AHCT1G125 -> R9(470R) -> LED7 -> LED6 -> ... -> LED1`。
- 原理图当前存在网络错误：U3 输入与 PA4 使用 `RGB控制`，U3/R9 输出使用 `灯控`，但 LED7 DIN 又被标成 `RGB控制`。因此 LED7 实际绕过了 5V 电平转换，而 `灯控` 输出悬空。5V 供电的 SK6805 不能保证识别 PA4 的 3.3V 高电平，这与“烧录后七颗灯完全无现象”一致。
- 软件逻辑映射暂定：LED1~LED5 对应相机 1~5，LED6 对应鱼眼，LED7 对应系统状态。首次样板验证时必须核对丝印与实际位置；如果布局定义不同，只修改 `bsp_led_handler.c` 中的映射表。

## 2. 软件分层

```text
M0_ing/
├── Core/                              CubeMX 生成层，只放启动和外设初始化
├── Drivers/                           STM32 HAL/CMSIS
├── BSP/
│   └── LED/
│       ├── bsp_led_driver.c/.h        PA4、SK6805 时序、GRB 帧缓存
│       └── bsp_led_handler.c/.h       相机/鱼眼/系统灯逻辑映射
├── Middlewares/
│   └── FreeRTOS/
│       ├── FreeRTOSConfig.h           STM32F042 资源与中断适配
│       └── os_hooks.c                 内存/栈故障钩子
├── User_tasks/
│   ├── led_task.c/.h                  LED 任务
│   ├── beep_task.c/.h                 蜂鸣器任务
│   └── task_manager.c/.h              应用任务集中注册
└── docs/
    └── DEVELOPMENT_PLAN.md
```

依赖方向固定为：`User_tasks -> BSP handler -> BSP driver -> HAL`。BSP 不创建任务，任务层不直接操作 GPIO。后续每个外设继续使用同样的 `driver/handler` 对，例如 `BSP/BEEP`、`BSP/Button`、`BSP/Hub`（仅在硬件信号补齐后）。

## 3. 分阶段开发与任务划分

### 阶段 A：工程基线与板级启动

1. 固定 CubeMX 版本、HAL 包版本、GCC 版本和 CMake preset。
2. 验证 HSI48、SWD、TIM1 HAL tick、启动文件和链接脚本。
3. 建立 Debug/Release 构建与 Flash/RAM 门限检查。
4. 产出：可重复构建的空工程、时钟测量记录、固件版本信息。

### 阶段 B：FreeRTOS 最小移植

1. 选择 FreeRTOS GCC Cortex-M0 port。
2. SysTick 专供 FreeRTOS；HAL 继续使用 TIM1 产生 1 ms tick。
3. 按 6 KB SRAM 约束设置 2 KB FreeRTOS heap、64-word idle stack，并打开 malloc/stack overflow 故障钩子。
4. 建立唯一的 `task_manager_init()` 任务注册入口。
5. 验证 1 ms tick、任务切换、延时精度、任务栈余量与长期运行。

### 阶段 C：7 颗 RGB 灯与 5 路设备灯（当前已实现）

1. Driver：发送 SK6805 的 24-bit GRB、帧缓存、锁存复位、临界区保护。
2. Handler：把相机 1~5、鱼眼、系统状态映射到物理串联序号。
3. Task：循环执行全红、全绿、全蓝、五路相机跑马灯、鱼眼灯、系统灯闪烁和组合灯效。
4. 板测：逻辑分析仪测 PA4 或 R9 后端，检查 `T0H≈0.3 us`、`T1H≈0.9 us`、码元周期约 `1.2 us`、复位低电平大于 `200 us`。
5. 逐灯发送红、绿、蓝测试帧，核对颜色顺序和 LED1~LED7 物理编号。
6. 在验证软件前先修正灯控网络：断开 LED7 DIN 与 MCU 侧 `RGB控制` 的连接，并把 R9 输出侧 `灯控` 接至 LED7 DIN。不能直接把 `灯控` 与 `RGB控制` 短接，否则 5V 缓冲输出可能向 MCU PA4 回灌。

### 阶段 D：基础板级外设

1. 蜂鸣器：Timer PWM driver；handler 提供短鸣、告警节奏；禁止任务层使用忙等待翻转 GPIO。
2. SW1/SW2：GPIO/EXTI driver；handler 去抖、短按/长按；通过队列或任务通知上报事件。
3. UART：日志与生产测试协议；中断/DMA 接收与环形缓冲分离。
4. 系统状态：上电、自检、运行、相机异常、致命故障对应统一灯色和蜂鸣规则。

### 阶段 E：USB Hub 与六相机联调

1. 上位机确认 6 个 UVC 设备同时枚举，记录 VID/PID、序列号、端口路径。
2. 计算 USB 2.0 上行带宽；按分辨率、帧率、像素格式评估 6 路同时采集是否超带宽。
3. 做单路、五路、六路阶梯压力测试，记录掉帧、重连和电源压降。
4. 进行热插拔、反复上电、ESD、线缆差异和 8 小时稳定性测试。
5. 若需要固件控制 Hub/相机电源，下一版硬件应把 CH338F `RESET#/PWREN#/OVCUR#` 及 U5/U6 EN 接入 MCU；若要求逐相机开关，还需每路独立负载开关。

### 阶段 F：产品化

1. 看门狗、故障码、复位原因、运行统计和版本读取。
2. 生产测试模式：逐灯、蜂鸣器、按键、六路 USB 端口和供电测试。
3. 静态分析、栈高水位、边界测试、断电恢复和升级策略。
4. 输出接口文档、测试报告、烧录文件与发布记录。

## 4. 当前固件构建与板测

在 `M0_ing` 下构建：

```powershell
cmake --preset Debug
cmake --build --preset Debug --parallel
```

如果工程目录移动过，旧 `build/Debug/CMakeCache.txt` 会保留原绝对路径。可使用新的构建目录，或在确认无须保留旧产物后删除旧构建目录再重新配置。

### 当前代码的实验现象

当前版本运行开发阶段的循环灯链自检，不是最终产品状态机。一次正常循环如下：

1. 上电/复位后 PA4 保持低电平，7 颗灯应为熄灭状态。
2. FreeRTOS 启动 LED 任务后，先发送一次全灭帧。
3. 七颗灯全红约 500 ms、全绿约 500 ms、全蓝约 500 ms，然后全灭约 250 ms。
4. 相机 1~5 对应灯珠依次白色点亮，每颗约 250 ms。
5. LED6（鱼眼）单独显示紫色约 500 ms。
6. LED7（系统状态）单独显示青色并闪烁三次，亮灭半周期约 200 ms。
7. 最后五路相机灯绿色、鱼眼灯蓝色、系统灯白色，同时保持约 1.2 s。
8. 上述灯效不断循环。它仅用于硬件自检，不代表最终烧录/运行/故障状态定义。

若硬件网络修正后仍无现象，按以下顺序测量：

1. LED 电源对地是否约为 5V。
2. PA4 是否存在一帧约 168 bit 的波形。
3. U3 的 A 输入是否跟随 PA4，OE# 是否为低，VCC 是否为 5V。
4. R9 输出是否出现约 0~5V 的数据波形，并且该波形是否真正到达 LED7 DIN。
5. 检查 `T0H`、`T1H`、码元周期和大于 200 us 的复位低电平。

若亮灯位置反向或错位，先核对串联方向，再调整 handler 映射，不要修改底层发送时序。

## 5. 验收标准

- Debug 与 Release 均无编译警告/错误，Flash 和 SRAM 不越界。
- FreeRTOS 能连续运行，1 ms tick 正常，malloc/stack fault 不触发。
- 7 颗灯可独立显示 RGB，五路相机灯逻辑编号与实物一致。
- 六路相机由上位机稳定枚举，带宽和供电压力测试有数据记录。
- 上层任务不直接依赖 HAL；更换 GPIO、灯珠或 RTOS 时只影响对应适配层。
