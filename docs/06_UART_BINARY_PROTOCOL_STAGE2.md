> 文档编号：06

# UART二进制通信协议——阶段2移植与验收

## 1. 本阶段目标

阶段2只建立一条可长期扩展的“MCU与PC二进制通信链路”，暂不接入ROS 2和micro-ROS。完成后，STM32能够在USART2 DMA字节流上完成：

- 帧头识别、协议版本检查和小端字段解析；
- CRC16校验；
- 半包、粘包和前导噪声处理；
- 错误帧丢弃及后续合法帧自动重新同步；
- HELLO、PING、STATUS和命令错误响应；
- 通信收发、CRC错误、格式错误和TX错误统计。

阶段3才会在WSL PC端把这些二进制消息映射成ROS 2 Topic。这样MCU不承担micro-ROS运行时，可控制Flash和6 KB SRAM占用，也便于独立排查UART和ROS两层问题。

## 2. 软件分层

```text
PC阶段2测试脚本
        |
        | USART2 / 115200 / 8N1
        v
User_tasks/communication_task.c       命令分发、响应、统计
        |
Middlewares/Protocol/mcu_protocol.c   帧、CRC、流式解析
        |
BSP/UART/bsp_uart_driver.c            HAL UART DMA收发
        |
STM32 HAL + USART2 + DMA
```

`mcu_protocol.c` 不依赖HAL、FreeRTOS、UART或ROS 2，因此可以直接在Windows主机上做单元测试。`communication_task.c` 是UART BSP在应用层的唯一使用者，避免多个任务同时操作串口造成竞争。

## 3. 帧格式

一帧最短10字节，Payload最大64字节，一帧最大74字节。所有多字节整数使用小端字节序。

| 偏移 | 长度 | 字段 | 说明 |
| --- | ---: | --- | --- |
| 0 | 1 | SOF0 | 固定 `0xAA` |
| 1 | 1 | SOF1 | 固定 `0x55` |
| 2 | 1 | Version | 当前固定 `0x01` |
| 3 | 1 | Message Type | 消息类型 |
| 4 | 2 | Sequence | 请求序号；响应原样返回 |
| 6 | 2 | Payload Length | 0～64 |
| 8 | N | Payload | 消息数据 |
| 8+N | 2 | CRC16 | 低字节在前 |

CRC覆盖范围是从 `Version` 到Payload最后一个字节，不包含两个帧头字节，也不包含CRC本身。

CRC算法采用CRC16-CCITT-FALSE：

- Polynomial：`0x1021`
- Initial Value：`0xFFFF`
- RefIn / RefOut：`false / false`
- XorOut：`0x0000`
- 标准测试串 `123456789` 的结果：`0x29B1`

## 4. 阶段2消息定义

| 方向 | Type | 名称 | Payload |
| --- | ---: | --- | --- |
| PC→MCU | `0x01` | HELLO_REQ | 必须为空 |
| PC→MCU | `0x02` | PING_REQ | 0～64字节，内容任意 |
| PC→MCU | `0x03` | STATUS_REQ | 必须为空 |
| MCU→PC | `0x81` | HELLO_RESP | 协议、能力和固件标签 |
| MCU→PC | `0x82` | PING_RESP | 原样回显PING Payload |
| MCU→PC | `0x83` | STATUS_RESP | 状态和通信统计 |
| MCU→PC | `0xFF` | ERROR_RESP | 两字节命令级错误 |

### 4.1 HELLO_RESP Payload

| 偏移 | 类型 | 含义 |
| --- | --- | --- |
| 0 | `uint8` | 协议版本，当前为1 |
| 1 | `uint8` | 设备类型，TacGlove控制板为1 |
| 2 | `uint16` | 最大Payload，当前为64 |
| 4 | `uint32` | 能力位图 |
| 8 | `uint8` | 固件标签长度L |
| 9 | `uint8[L]` | ASCII固件标签，当前为 `STAGE2_R1` |

能力位：bit0 PING、bit1 STATUS、bit2 CRC16、bit3 Sequence、bit4流式重同步。

### 4.2 STATUS_RESP Payload

Payload固定32字节：

| 偏移 | 类型 | 含义 |
| --- | --- | --- |
| 0 | `uint8` | 系统状态 |
| 1 | `uint8` | 保留，当前为0 |
| 2 | `uint16` | 故障码 |
| 4 | `uint32` | 上电运行时间，毫秒 |
| 8 | `uint32` | 成功接收的合法帧数 |
| 12 | `uint32` | 成功发送的响应帧数；当前STATUS响应尚未计入本字段 |
| 16 | `uint32` | CRC错误帧数 |
| 20 | `uint32` | 版本/长度等格式错误数 |
| 24 | `uint32` | TX错误数 |
| 28 | `uint32` | UART接收总字节数 |

系统状态值：0自检、1待机、2数采准备、3数采中、4故障。故障码定义集中在 `User_tasks/system_status.h`。

### 4.3 ERROR_RESP Payload

| 偏移 | 类型 | 含义 |
| --- | --- | --- |
| 0 | `uint8` | 错误码：1不支持的消息；2 Payload非法 |
| 1 | `uint8` | 导致错误的请求消息类型 |

只有“帧本身通过CRC，但命令不合法”时才回复ERROR。CRC错误帧和格式错误帧直接丢弃，避免对线路噪声发送无意义响应。

## 5. 流式接收行为

USART2 RX DMA使用256字节循环缓冲区；115200 baud下约可缓存22 ms数据，并能容纳3个完整最大帧。通信任务每2 ms读取一次DMA新增数据，每次最多处理24字节。

解析器逐字节运行，因此支持：

- 一帧分多次到达；
- 多帧一次到达；
- 帧前存在任意噪声；
- Payload内部包含 `AA 55`；
- CRC或长度错误后继续查找下一组 `AA 55`。

本阶段使用“一请求一响应”。PC必须等待匹配Sequence的响应后再发送下一条压力测试请求，后续有持续状态上报需求时再增加发送队列。

## 6. 内存与RTOS调整

- FreeRTOS heap：从2048字节调整为2304字节；
- CubeMX/newlib最小heap：从512字节调整为0；当前业务不调用 `malloc/calloc`，FreeRTOS继续使用独立的 `heap_4`；
- 通信任务栈：80 words，即320字节；
- RX DMA循环缓冲：从128字节调整为256字节；
- 协议最大Payload：64字节；
- 协议解析器、TX帧和统计均为静态内存，不调用 `malloc`。

当前Release链接结果：

- RAM：4456 / 6144字节，72.53%，剩余1688字节；
- Flash：15876 / 32768字节，48.45%，剩余16892字节。

链接结果已经把FreeRTOS heap、任务静态数据、DMA缓冲以及链接脚本保留的1 KB主/中断栈计算在RAM内。FreeRTOS任务控制块和各任务栈会在运行时从这2304字节heap中分配；本阶段增加通信任务栈的同时增加了heap预算。newlib heap的最小预留改为0后，不应在业务代码中直接调用 `malloc/calloc`；如确有动态内存需求，应优先评估FreeRTOS内存接口并重新核算峰值。后续每增加任务，都必须重新检查链接占用和 `xTaskCreate` 返回值，不能只看Flash。

## 7. 构建与烧录

在Windows PowerShell中逐条执行：

```powershell
cd D:\M0\five_data\qyan_Tacglove
```

```powershell
cmake --preset Release
```

```powershell
cmake --build --preset Release --target flash
```

`flash` 会先构建再烧录本次Release标准ELF。也可以先只构建，手工烧录：

```powershell
cmake --build --preset Release
```

```powershell
openocd -f "openocd.cfg" -c "gdb port disabled" -c "init; reset init" -c "program build/cmake/Release/M0_Init_STAGE2_R1.elf verify reset exit"
```

本阶段必须烧录Release。烧录后旧的 `uart_dma_stage1_test.py` 会因固件不再解析ASCII命令而超时，这是正常现象，不代表UART退化。

## 8. WSL验收步骤

如果WSL重启后没有 `/dev/ttyUSB0`，先在Windows管理员PowerShell中重新执行usbipd attach，再在WSL确认：

```bash
ls -l /dev/ttyUSB0
```

先做10次测试：

```bash
python3 /mnt/d/M0/five_data/AppEncrypt/uart_protocol_stage2_test.py \
  /dev/ttyUSB0 \
  --count 10
```

通过后做1000次压力测试：

```bash
python3 /mnt/d/M0/five_data/AppEncrypt/uart_protocol_stage2_test.py \
  /dev/ttyUSB0 \
  --count 1000
```

脚本依次验证：本地CRC向量、HELLO、STATUS、逐字节半包、粘包、坏CRC后恢复、非法长度后恢复、未知命令、错误Payload以及PING压力测试。

脚本默认要求固件标签为 `STAGE2_R1`，可以防止再次误烧阶段1或其他目录中的旧ELF。以后有意测试其他版本时，可显式增加 `--expected-tag 新标签`。

最终应看到：

```text
PASS: 阶段2二进制协议测试通过
```

同时最终STATUS应满足CRC错误至少1次、格式错误至少1次、TX错误为0。请把完整终端输出发回，再决定是否进入阶段3。

## 9. 阶段2验收标准

- Release固件构建和OpenOCD verify成功；
- HELLO返回协议版本1和固件标签 `STAGE2_R1`；
- 10/10功能测试通过；
- 1000/1000 PING压力测试通过；
- 半包、粘包和错误帧恢复测试全部通过；
- 最终TX错误计数为0；
- 按键、LED和蜂鸣器原业务状态机仍正常工作。

## 10. CubeMX同步清单

本阶段没有修改UART、DMA、GPIO或NVIC外设配置，但为了避免与FreeRTOS `heap_4` 重复占用RAM，修改了一个CubeMX工程参数：

- Project Manager → Project → Linker Settings → Minimum Heap Size：`0x200` 改为 `0x0`；
- Minimum Stack Size继续保持 `0x400`；
- Generate Code后确认 `M0_Init.ioc` 中为 `ProjectManager.HeapSize=0x0`，链接脚本中为 `_Min_Heap_Size = 0x0`。

其余继续保持阶段1已经验证的设置：

- USART2：Asynchronous，115200，8 data bits，1 stop bit，no parity；
- PA2为USART2_TX，PA3为USART2_RX；
- USART2_RX DMA：Circular，High；
- USART2_TX DMA：Normal，High；
- DMA1 Channel 4_5 IRQ：Priority 3；
- USART2 IRQ：Priority 3；
- Project Manager → Code Generator → Delete previously generated files when not re-generated：关闭。

本阶段发生但不属于CubeMX的源码配置变化：RX DMA软件缓冲改为256字节、FreeRTOS heap改为2304字节、通信任务栈改为320字节、固件标签改为 `STAGE2_R1`。CubeMX重新生成后需要确认这些自定义文件和顶层 `CMakeLists.txt` 仍被保留并参与构建。
