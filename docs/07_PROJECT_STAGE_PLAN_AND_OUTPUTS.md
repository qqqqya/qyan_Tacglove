> 文档编号：07

# TacGlove项目阶段计划与已完成产物

## 1. 总体技术路线

本项目采用路线A：STM32F042运行FreeRTOS和轻量UART二进制协议，ROS 2节点运行在WSL PC端。MCU不直接链接micro-ROS库，也不需要运行 `micro_ros_agent`。

```text
PB6 / SK6805 / 蜂鸣器
        |
STM32F042 + FreeRTOS
        |
USART2 DMA + 自定义二进制帧
        |
CP210x / /dev/ttyUSB0
        |
WSL: tacglove_uart_bridge (rclpy)
        |
ROS 2 Topic
        |
后续六相机采集与录制程序
```

这样划分的原因是STM32F042只有32 KB Flash和6 KB SRAM。ROS 2类型系统、DDS/XRCE运行时和消息内存全部留在PC端，MCU只处理确定长度的小端帧、CRC和业务状态机。

## 2. 阶段状态总表

| 阶段 | 内容 | 当前状态 | 验收结果 |
| --- | --- | --- | --- |
| 基础阶段 | CMake/CubeMX、FreeRTOS、LED、蜂鸣器、PB6、状态机 | 已完成 | 板级灯效、按键和蜂鸣流程已建立 |
| 阶段1 | USART2 RX/TX DMA与ASCII握手/PING | 已完成 | Release固件1000/1000往返通过 |
| 阶段2 | 正式二进制帧、CRC16、半包/粘包/错误恢复 | 已完成 | 10/10和1000/1000均通过，TX错误为0 |
| 阶段3 | WSL ROS 2消息、UART桥、按键事件和业务命令 | 已编程，待板测 | 本地构建与8项测试通过，等待STAGE3_R1联调 |
| 阶段4 | 单相机枚举、稳定识别和录制闭环 | 未开始 | 等阶段3通过后开始 |
| 阶段5 | 五路普通相机+一路鱼眼并发录制 | 未开始 | 带宽、掉帧、时间戳和落盘验收 |
| 阶段6 | 故障恢复、看门狗、长期稳定性与发布 | 未开始 | 8小时测试、异常注入和发布记录 |

## 3. 基础阶段：板级驱动与FreeRTOS

已完成内容：

- SK6805串行RGB底层时序和七灯逻辑映射；
- LED1～LED5普通相机、LED6鱼眼、LED7系统灯的分层接口；
- 蜂鸣器Driver/Handler；
- PB6输入上拉、20 ms扫描、约60 ms消抖；
- FreeRTOS Cortex-M0 GCC port、TIM1 HAL tick、SysTick RTOS tick；
- 上电自检、待机、准备、采集、故障状态机；
- 开始/结束短鸣以及相机功能TODO占位。

主要固件文件：

| 文件/目录 | 用途 |
| --- | --- |
| `BSP/LED/` | SK6805时序、帧缓存和七灯逻辑映射 |
| `BSP/BEEP/` | 蜂鸣器硬件封装 |
| `BSP/KEY/` | PB6低电平有效读取Handler |
| `Middlewares/FreeRTOS/` | M0裁剪配置和故障钩子 |
| `User_tasks/led_task.c` | LED、蜂鸣器和采集业务状态机 |
| `User_tasks/key_task.c` | 按键扫描、消抖和事件产生 |
| `User_tasks/system_event.c` | 跨任务控制/观察队列 |
| `User_tasks/system_status.c` | 可供通信读取的原子状态快照 |
| `User_tasks/task_manager.c` | 任务和队列集中创建入口 |

## 4. 阶段1：UART DMA基线

目标是先排除CP210x、WSL设备映射、波特率、DMA和HAL状态恢复问题，不涉及ROS 2。

已产生文件：

| 文件 | 用途 |
| --- | --- |
| `BSP/UART/bsp_uart_driver.c/.h` | USART2 RX循环DMA、TX DMA和发送完成恢复 |
| `User_tasks/uart_test_task.c/.h` | 阶段1 ASCII HELLO/PING历史测试任务；保留但不再编译 |
| `AppEncrypt/uart_dma_stage1_test.py` | 阶段1 WSL压力测试脚本 |
| `docs/04_UART_DMA_STAGE1.md` | 接线、权限、命令和故障排查记录 |

阶段1最终证明Release固件能够完成1000/1000 UART往返。Debug ELF失败而Release版本成功的问题也已定位为构建类型和任务栈/时序差异，不是版本化文件名导致。

## 5. 阶段2：正式二进制协议

已完成内容：

- 固定帧头 `AA 55`、协议版本、消息类型、Sequence和Payload Length；
- 最大Payload 64字节；
- CRC16-CCITT-FALSE；
- 流式解析、噪声过滤、半包、粘包、长度错误和CRC错误恢复；
- HELLO、PING、STATUS和ERROR消息；
- 固件标签 `STAGE2_R1` 和版本化构建产物。

已产生文件：

| 文件 | 用途 |
| --- | --- |
| `Middlewares/Protocol/mcu_protocol.c/.h` | 与HAL/RTOS无关的协议编解码器 |
| `User_tasks/communication_task.c/.h` | UART协议任务；阶段3继续扩展此任务 |
| `tests/mcu_protocol_host_test.c` | Windows主机端C协议单元测试 |
| `AppEncrypt/uart_protocol_stage2_test.py` | WSL串口协议及1000次压力测试 |
| `docs/06_UART_BINARY_PROTOCOL_STAGE2.md` | 帧定义、内存和验收文档 |

用户目标板验收结果：

- 10/10：平均8.041 ms，最大8.690 ms；
- 1000/1000：平均8.400 ms，最大22.862 ms；
- 最终统计：CRC错误2、格式错误2、TX错误0；
- 主动注入的坏CRC和非法长度均被正确记录，后续好帧恢复成功。

## 6. 阶段3：ROS 2-UART桥接

阶段3已经完成代码实现，等待烧录 `STAGE3_R1` 后由目标板验收。新增能力：

- PB6事件通过独立观察队列上报，不会被通信任务抢走LED控制事件；
- 按键事件明确区分“请求开始”和“请求停止”，避免PC自行猜测Toggle状态；
- ROS命令支持开始、停止和短鸣；
- 命令ACK只表示已接收，最终结果通过10 Hz状态Topic确认；
- STATUS新增FreeRTOS剩余heap，便于板上监测6 KB SRAM；
- 串口断开自动重试；固件标签或能力不匹配时拒绝进入Ready；
- UART只能由桥接节点独占，避免多个脚本同时读取造成丢帧。

阶段3涉及的MCU文件：

| 文件 | 用途 |
| --- | --- |
| `Middlewares/Protocol/mcu_protocol.h` | 增加命令、命令响应和异步按键消息定义 |
| `User_tasks/system_event.c/.h` | 控制队列与观察队列双路分发，避免一个事件被两个任务争抢 |
| `User_tasks/key_task.c` | 把PB6动作转成明确的开始/停止事件并记录MCU时间戳 |
| `User_tasks/led_task.c` | 执行开始、停止、短鸣事件并维护灯效状态机 |
| `User_tasks/communication_task.c/.h` | 解析PC命令、发布PB6事件、10 Hz状态及运行时heap |
| `Middlewares/FreeRTOS/FreeRTOSConfig.h` | FreeRTOS heap由2304调整为2560字节 |
| `CMakeLists.txt` | 版本标签和产物名更新为 `STAGE3_R1` |

新增PC/ROS文件位于 `AppEncrypt/ros2_ws/src`：

| 文件/目录 | 用途 |
| --- | --- |
| `tacglove_msgs/msg/ButtonEvent.msg` | PB6开始/停止事件 |
| `tacglove_msgs/msg/MCUStatus.msg` | MCU状态、通信统计和FreeRTOS heap |
| `tacglove_msgs/msg/DeviceCommand.msg` | PC下发开始/停止/短鸣 |
| `tacglove_msgs/msg/CommandResult.msg` | MCU ACK与桥接层错误 |
| `tacglove_uart_bridge/protocol.py` | PC侧帧、CRC和Payload编解码 |
| `tacglove_uart_bridge/bridge_node.py` | `/dev/ttyUSB0`与ROS Topic桥接 |
| `tacglove_uart_bridge/stage3_test.py` | 自动命令测试和PB6交互验收 |
| `tacglove_uart_bridge/test/test_protocol.py` | PC协议单元测试 |
| `tacglove_uart_bridge/test/test_bridge_mapping.py` | 不接板时验证HELLO、STATUS、命令和按键映射 |

本地验收结果：

- MCU Release构建成功：RAM 4720/6144字节（76.82%），Flash 16372/32768字节（49.96%）；
- 固件协议主机端C测试通过；
- ROS 2代码规范检查通过；
- `colcon build` 两个包构建成功；
- `colcon test-result` 为8 tests、0 errors、0 failures、0 skipped。

阶段3的完整操作见 `docs/08_ROS2_UART_BRIDGE_STAGE3.md`。

## 7. 阶段4：单相机采集闭环

阶段3通过后再开始，避免同时调试ROS、串口和UVC。任务划分：

1. PC端枚举一个普通USB 2.0相机，记录VID、PID、序列号和USB端口路径；
2. 固定设备标识，不使用容易随插拔变化的 `/dev/videoN` 作为唯一身份；
3. 获取分辨率、帧率、格式和实际带宽；
4. 建立独立相机Worker，支持prepare/start/stop/finalize；
5. PB6 START事件触发PC准备相机，准备完成后进入录制；
6. PB6 STOP事件触发停止、刷新缓冲和关闭文件；
7. 发布相机在线、帧计数、丢帧数和写盘错误，异常时反馈MCU进入故障状态。

期望产物：相机设备清单、单相机录制程序、状态消息、单路30分钟稳定性记录。

## 8. 阶段5：六相机并发采集

1. 依次接入五路普通相机和一路鱼眼；
2. 固定六路物理端口到逻辑相机ID；
3. 做1、2、3、6路阶梯带宽测试；
4. 统一录制Session ID和PC时间戳；
5. 独立采集线程/进程与有界写盘队列，禁止慢磁盘阻塞取帧；
6. 检测丢帧、USB断连、磁盘不足和文件关闭失败；
7. 只有全部相机准备完成，LED7才从蓝色闪烁切换为蓝色常亮。

## 9. 阶段6：产品化

- 通信超时和相机故障码；
- PC断连后的安全停止策略；
- 独立看门狗、复位原因和故障保留；
- 录制文件校验、断电恢复和磁盘空间保护；
- 8小时六路压力测试、重复插拔、重复启动/停止；
- 固件、ROS包、配置文件和测试报告版本化发布。

## 10. 持续约束

- MCU板上验收统一烧录Release，不使用Debug作为运行版本；
- CubeMX重新生成后必须核对每阶段文档中的同步清单；
- `User_tasks -> BSP Handler -> BSP Driver -> HAL`依赖方向不反转；
- 相机属于PC端资源，MCU不负责UVC视频搬运；
- 同一时刻只有一个程序打开 `/dev/ttyUSB0`；
- 每阶段通过后再进入下一阶段，未通过时保留终端完整输出进行定位。
