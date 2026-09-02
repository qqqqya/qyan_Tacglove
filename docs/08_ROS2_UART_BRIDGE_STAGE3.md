> 文档编号：08

# ROS 2与UART桥接——阶段3编程及验收

## 1. 阶段目标与边界

阶段3把已经验证的UART二进制链路接入WSL中的ROS 2 Jazzy，同时验证PB6、LED和蜂鸣器的完整控制闭环。本阶段仍不连接相机，也不启动实际视频录制。

本项目采用路线A：

- STM32只运行FreeRTOS、状态机和轻量二进制协议；
- WSL运行普通 `rclpy` 节点并打开 `/dev/ttyUSB0`；
- ROS消息在PC端生成和序列化；
- 不在STM32中链接micro-ROS；
- 不需要执行 `micro_ros_agent serial ...`。

参考工程中的G4+micro-ROS代码保持只读，仅用于理解消息职责。本项目重新定义了更符合相机采集业务、也更适合M0资源限制的接口。

## 2. 通信结构

```text
ROS 2业务节点
   |  /tacglove/mcu/command
   |  /tacglove/mcu/command_result
   |  /tacglove/mcu/status
   |  /tacglove/mcu/button_events
   v
tacglove_uart_bridge（WSL、rclpy、串口唯一拥有者）
   |
   | AA55二进制帧 + Sequence + CRC16
   v
communication_task（STM32）
   |                       |
   | control queue         | observer queue
   v                       v
led_task                PB6异步上报
```

物理按键事件会复制到两个队列：LED状态机消费控制副本，通信任务消费观察副本。因此通信任务不会抢走本应由LED任务处理的按键事件。ROS命令只进入控制队列，不会伪装成物理PB6事件。

## 3. ROS 2接口

节点名：`tacglove_uart_bridge`

| Topic | 消息 | 方向 | 频率/触发 | 用途 |
| --- | --- | --- | --- | --- |
| `/tacglove/mcu/button_events` | `tacglove_msgs/msg/ButtonEvent` | MCU→ROS | PB6事件驱动 | 请求开始或停止采集 |
| `/tacglove/mcu/status` | `tacglove_msgs/msg/MCUStatus` | MCU→ROS | 默认10 Hz | 状态、故障、UART统计、heap |
| `/tacglove/mcu/command` | `tacglove_msgs/msg/DeviceCommand` | ROS→MCU | 事件驱动 | 开始、停止、短鸣 |
| `/tacglove/mcu/command_result` | `tacglove_msgs/msg/CommandResult` | MCU→ROS | 每条命令一次 | ACK、忙、队列满、超时等 |

### 3.1 ButtonEvent

重要字段：

- `button_id=1`：PB6采集按键；
- `event_type=1`：明确请求开始采集；
- `event_type=2`：明确请求停止采集；
- `event_sequence`：MCU生成的16位事件序号；
- `mcu_uptime_ms`：按键完成消抖时的MCU单调时间；
- `system_state_before`：事件产生前的业务状态。

没有使用含义不确定的Toggle消息。PC收到START或STOP后能够执行明确、幂等的动作。

### 3.2 MCUStatus

状态值：

- 0：上电自检；
- 1：待机；
- 2：采集准备；
- 3：采集中；
- 4：故障。

消息还包含固件标签、故障码、UART收发帧数、CRC错误、格式错误、TX错误、接收字节数和 `free_rtos_heap_bytes`。状态Topic由桥接节点每100 ms查询一次MCU后发布，不是伪造的PC状态。

### 3.3 DeviceCommand与CommandResult

当前命令：

- 1：`COMMAND_CAPTURE_START`；
- 2：`COMMAND_CAPTURE_STOP`；
- 3：`COMMAND_BEEP_SHORT`。

每条命令必须带 `command_id`，ACK会返回相同ID。ACK结果：

- 0：已进入控制队列；
- 1：已经处于目标状态，按幂等成功处理；
- 2：当前状态忙；
- 3：MCU控制队列满；
- 4：MCU不支持该命令；
- 250：桥尚未Ready；
- 251：UART响应超时；
- 252：UART或协议IO错误。

ACK=0只表示“MCU已经接收并入队”，不表示准备灯效和采集动作已经完成。上位机必须继续观察 `/tacglove/mcu/status`，确认状态真正变为3或1。

## 4. 阶段3 UART新增帧

阶段2帧头、Sequence、最大64字节Payload和CRC算法保持不变。

| Type | 名称 | 方向 |
| ---: | --- | --- |
| `0x10` | COMMAND_REQ | PC→MCU |
| `0x40` | BUTTON_EVENT | MCU→PC，异步 |
| `0x90` | COMMAND_RESP | MCU→PC |

COMMAND_REQ Payload固定6字节：

| 偏移 | 类型 | 含义 |
| --- | --- | --- |
| 0 | `uint8` | command |
| 1 | `uint8` | reserved，必须为0 |
| 2 | `uint32` | command_id |

COMMAND_RESP Payload固定9字节：command、result、command_id、ACK时系统状态、故障码。

BUTTON_EVENT Payload固定7字节：button_id、event_type、mcu_uptime_ms、事件前系统状态；帧Sequence就是事件序号。

STATUS Payload由阶段2的32字节扩展为36字节，末尾新增 `uint32 free_rtos_heap_bytes`。

## 5. 业务现象

上电和PB6本地逻辑继续由MCU独立运行，即使ROS 2暂时未启动也能工作：

1. 上电：LED1～LED6绿色闪烁3次；
2. 自检通过：LED1～LED6绿色常亮、LED7熄灭，状态1；
3. PB6或ROS START：LED7蓝色闪烁3次、蜂鸣器短响、LED7蓝色常亮，状态3；
4. PB6或ROS STOP：停止占位流程、蜂鸣器短响、LED7熄灭，状态1；
5. ROS BEEP_SHORT：只短鸣，不改变LED和系统状态；
6. 自检、准备或故障期间按PB6会被忽略，避免状态机重入。

LED颜色是系统真实状态的显示结果，因此没有照搬参考项目中的 `led_mode` 任意覆盖接口。

## 6. 已完成的本地验证

- STM32 Release构建成功；
- 固件版本：`STAGE3_R1`；
- RAM：4720 / 6144字节，76.82%，剩余1424字节；
- Flash：16372 / 32768字节，49.96%，剩余16396字节；
- 通信任务静态栈帧112字节，任务栈配置320字节；
- FreeRTOS heap配置2560字节，实际运行余量由STATUS上报；
- ROS 2 Jazzy能够发现两个新包；
- Python代码规范检查：6个文件，No problems found；
- `colcon test-result`：8 tests，0 errors，0 failures，0 skipped；
- 桥接节点在串口不存在时能够正常启动并进入自动重试。

这些结果不代替目标板验收。UART命令、PB6异步帧和运行时heap必须在烧录后验证。

## 7. 第一次构建ROS 2工作区

请使用普通用户 `embedded` 打开WSL，不再使用root。先确认 `id` 输出包含 `dialout`。

逐条执行：

```bash
source /opt/ros/jazzy/setup.bash
```

```bash
cd /mnt/d/M0/five_data/AppEncrypt/ros2_ws
```

```bash
colcon build
```

```bash
source install/setup.bash
```

验证包：

```bash
ros2 pkg prefix tacglove_msgs
```

```bash
ros2 pkg prefix tacglove_uart_bridge
```

两个命令都应返回 `.../AppEncrypt/ros2_ws/install/...` 路径。

每次修改 `.msg`、`setup.py` 或桥接源码后重新执行 `colcon build`。每次新开WSL终端都要重新source ROS和工作区。

## 8. 构建并烧录STAGE3_R1

在Windows PowerShell逐条执行：

```powershell
cd D:\M0\five_data\qyan_Tacglove
```

```powershell
cmake --preset Release
```

```powershell
cmake --build --preset Release --target flash
```

如果手工烧录：

```powershell
openocd -f "openocd.cfg" -c "gdb port disabled" -c "init; reset init" -c "program build/cmake/Release/M0_Init_STAGE3_R1.elf verify reset exit"
```

阶段3测试脚本要求HELLO返回 `STAGE3_R1`，误烧 `STAGE2_R1` 会明确报告固件不匹配。

## 9. 目标板自动验收

### 终端1：启动桥接节点

```bash
source /opt/ros/jazzy/setup.bash
```

```bash
source /mnt/d/M0/five_data/AppEncrypt/ros2_ws/install/setup.bash
```

```bash
export ROS_DOMAIN_ID=9
```

```bash
ros2 run tacglove_uart_bridge tacglove_uart_bridge --ros-args \
  -p port:=/dev/ttyUSB0 \
  -p baud:=115200 \
  -p expected_firmware_tag:=STAGE3_R1
```

期望日志：

```text
serial port opened: /dev/ttyUSB0
HELLO passed: tag=STAGE3_R1, capabilities=0x0000007F
```

保持终端1运行。

### 终端2：运行自动验收节点

```bash
source /opt/ros/jazzy/setup.bash
```

```bash
source /mnt/d/M0/five_data/AppEncrypt/ros2_ws/install/setup.bash
```

```bash
export ROS_DOMAIN_ID=9
```

```bash
ros2 run tacglove_uart_bridge tacglove_stage3_test
```

测试程序会自动完成：

1. 固件标签、连接、故障码和FreeRTOS heap检查；
2. 约10 Hz状态频率检查；
3. ROS START命令及状态3检查；
4. ROS短鸣命令；
5. ROS STOP命令及状态1检查；
6. 提示你按PB6开始，验证异步START事件；
7. 提示你再按PB6停止，验证异步STOP事件；
8. 最终故障码和UART TX错误检查。

最终应看到：

```text
PASS: 阶段3 ROS 2-UART桥接测试通过
```

请把终端1和终端2的完整输出一起发回。

## 10. 手工查看Topic

在已经source的第三个终端中可以执行：

```bash
ros2 topic list
```

```bash
ros2 topic echo /tacglove/mcu/status
```

```bash
ros2 topic hz /tacglove/mcu/status
```

```bash
ros2 topic echo /tacglove/mcu/button_events
```

手工短鸣：

```bash
ros2 topic pub --once /tacglove/mcu/command \
  tacglove_msgs/msg/DeviceCommand \
  "{command_id: 1, command: 3}"
```

查看命令结果：

```bash
ros2 topic echo /tacglove/mcu/command_result
```

## 11. 常见问题

### 串口Busy或握手异常

同一时刻只能有一个程序打开 `/dev/ttyUSB0`。运行桥接节点时必须关闭：

- `uart_protocol_stage2_test.py`；
- 串口助手；
- 其他Python串口脚本；
- 另一个桥接节点实例。

### 找不到tacglove_msgs

说明当前终端没有source工作区，重新执行：

```bash
source /opt/ros/jazzy/setup.bash
source /mnt/d/M0/five_data/AppEncrypt/ros2_ws/install/setup.bash
```

### 两个ROS终端互相看不到Topic

确认每个终端的 `ROS_DOMAIN_ID` 都是9：

```bash
echo $ROS_DOMAIN_ID
```

### WSL提示localhost代理未镜像

该提示与本地 `/dev/ttyUSB0`、本机ROS 2 Topic和UART握手无直接关系，本阶段可以忽略。以后需要联网安装包时再单独处理代理。

### colcon出现Clock skew警告

Windows和WSL文件时间可能短暂不同。只要最终Summary显示包构建成功即可；若出现实际编译失败，可关闭终端，在Windows执行 `wsl --shutdown` 后重新进入并构建。

## 12. CubeMX同步清单

阶段3没有新增或修改CubeMX外设配置：

- USART2和DMA参数保持阶段1设置；
- PB6继续为GPIO Input + Pull-up，不需要EXTI；
- 中断优先级不变；
- Minimum Stack Size继续为 `0x400`；
- Minimum Heap Size继续保持阶段2设置的 `0x0`。

本阶段的FreeRTOS heap从2304改为2560字节，这是 `Middlewares/FreeRTOS/FreeRTOSConfig.h` 的软件配置，不是CubeMX配置。CubeMX重新生成后需要确认自定义目录、顶层 `CMakeLists.txt` 和固件标签 `STAGE3_R1` 仍然保留。
