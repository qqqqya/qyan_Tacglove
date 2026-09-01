# STM32F042 FreeRTOS与CMake开发说明

本文既说明当前FreeRTOS移植内容，也用于把以前熟悉的“CubeMX + Keil + `freertos.c`”开发方式，对应到当前“CubeMX + CMake + 原生FreeRTOS API”工程。

## 1. 当前工程中的分层

```text
M0_ing/
├── Core/                         CubeMX生成的启动、时钟、GPIO和中断
├── Drivers/                      STM32 HAL与CMSIS
├── BSP/
│   ├── LED/                      SK6805物理驱动和板级逻辑映射
│   └── BEEP/                     蜂鸣器GPIO驱动和板级动作
├── Middlewares/
│   └── FreeRTOS/                 本机型FreeRTOSConfig和故障Hook
├── User_tasks/
│   ├── led_task.c/.h             LED自检任务
│   ├── beep_task.c/.h            蜂鸣器自检任务
│   └── task_manager.c/.h         所有任务的集中注册入口
├── CMakeLists.txt                用户源码、头文件目录和编译宏
└── cmake/stm32cubemx/CMakeLists.txt
                                  CubeMX生成源码和HAL源码列表
```

命名规则是“目录已经表达User/BSP/Middleware含义，文件名不重复堆叠目录名”。因此使用 `User_tasks/led_task.c`，不再使用 `User_tasks/user_led_task.c` 这类大全套名称。

## 2. 与以前CubeMX/Keil工程的对应关系

以前CubeMX开启FreeRTOS后，常见结构是：

```text
Core/Src/freertos.c
├── MX_FREERTOS_Init()
├── osThreadDef()/osThreadNew()
├── StartDefaultTask()
└── 任务句柄、队列句柄和信号量句柄
```

当前工程没有总的 `freertos.c`，不是缺文件，而是把其职责拆开了：

| 以前CubeMX/Keil中看到的内容 | 当前CMake工程对应位置 |
|---|---|
| `MX_FREERTOS_Init()` | `User_tasks/task_manager.c` 中的 `task_manager_init()` |
| `StartDefaultTask()` | `User_tasks/led_task.c`、`beep_task.c` 中各自的任务入口 |
| `osThreadNew()` / `osThreadCreate()` | 原生API `xTaskCreate()` |
| `osDelay()` | 原生API `vTaskDelay()` |
| `osMessageQueueNew()` | 原生API `xQueueCreate()`，需要时再增加 |
| CMSIS-RTOS任务属性结构 | 每个任务文件顶部的栈深度和优先级宏 |
| `FreeRTOSConfig.h` | `Middlewares/FreeRTOS/FreeRTOSConfig.h` |
| malloc/栈溢出回调 | `Middlewares/FreeRTOS/os_hooks.c` |
| Keil工程的Source Group | 磁盘目录 + `CMakeLists.txt` 的源码列表 |
| Keil的Include Paths | `target_include_directories()` |
| Keil的Define | `target_compile_definitions()` |
| Keil勾选参与编译的 `.c` | `target_sources()` |

当前启动过程为：

```text
Reset
  -> HAL_Init()
  -> SystemClock_Config()
  -> MX_GPIO_Init()
  -> task_manager_init()
       -> led_task_create()
       -> beep_task_create()
  -> vTaskStartScheduler()
       -> FreeRTOS自动创建Idle任务
       -> 进入任务调度
```

使用原生FreeRTOS API而不是CMSIS-RTOS封装，可以少一层适配，代码和RAM占用也更容易追踪。以后如果确实需要恢复 `freertos.c` 的阅读习惯，可以把 `task_manager.c` 理解为“拆分后的 `freertos.c` 总入口”，不要再另外创建第二套调度器入口。

## 3. 新增外设时是否必须修改CMakeLists

结论：新增 `.c` 文件通常必须加入CMake；新增 `.h` 文件本身不需要加入编译，但新头文件目录必须加入Include Path。

当前工程采用明确列出源码的方式，没有使用 `file(GLOB ...)`。这种方式能够防止临时文件被误编译，也能在代码审查时看清固件由哪些文件组成。

### 3.1 在已有目录增加 `.c/.h`

例如新增按键：

```text
BSP/KEY/bsp_key_driver.c
BSP/KEY/bsp_key_driver.h
User_tasks/key_task.c
User_tasks/key_task.h
```

在顶层 `CMakeLists.txt` 增加：

```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    BSP/KEY/bsp_key_driver.c
    User_tasks/key_task.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    BSP/KEY
    User_tasks
)
```

`User_tasks` 已经在Include Path中，所以再次添加不会出错，但没有必要。`.h` 不用写进 `target_sources()`。

### 3.2 增加一个新的应用任务

必须完成四步，少一步都可能出现“文件存在但任务不运行”：

1. 创建 `xxx_task.c/.h`；
2. 把 `xxx_task.c` 加入顶层 `target_sources()`；
3. 在 `task_manager.c` 调用 `xxx_task_create()`，并检查是否返回成功；
4. 重新配置、编译，并检查RAM及FreeRTOS heap。

示例：

```c
task_status_t result = key_task_create();
if (TASK_OK != result)
{
    /* 日志预留：记录按键任务注册失败及result。 */
    return result;
}
```

### 3.3 使用CubeMX增加UART、I2C、TIM等芯片外设

推荐流程：

1. 在 `.ioc` 中配置引脚、时钟、DMA和中断；
2. 让CubeMX重新生成CMake工程；
3. 检查 `Core/Src/usart.c` 等文件是否出现；
4. 检查 `cmake/stm32cubemx/CMakeLists.txt` 是否已把新文件和HAL Driver加入；
5. 用户的BSP、Task和Middleware仍放在顶层 `CMakeLists.txt`，不要塞进CubeMX生成区；
6. 重新执行Configure和Build。

CubeMX可能重新生成 `Core` 和 `cmake/stm32cubemx`。因此CubeMX文件中的人工代码只能放在 `USER CODE BEGIN/END` 区域；BSP和任务代码放在独立目录，可避免重新生成时丢失。

### 3.4 增加第三方库

第三方库除了 `.c` 和Include Path，还可能需要静态库和编译宏：

```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE Third_Party/example/example.c)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE Third_Party/example/include)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE EXAMPLE_FEATURE=1)
target_link_libraries(${CMAKE_PROJECT_NAME} example_library)
```

### 3.5 修改CMake后的操作

在 `M0_ing` 目录执行：

```powershell
cmake --preset Debug
cmake --build --preset Debug

cmake --preset Release
cmake --build --preset Release
```

只修改已登记的 `.c/.h` 时通常直接Build即可；增加文件、删除文件、修改路径或Include Path后，应先Configure再Build。VS Code显示了文件不代表它已经被编译，最终以Build输出和ELF符号为准。

## 4. 当前FreeRTOS移植了什么

- FreeRTOS Kernel V11.1.0；
- 完整内核位于工程外层 `../FreeRTOS`；
- 本目录保存当前STM32F042专用配置和故障Hook；
- 编译器端口为 `portable/GCC/ARM_CM0`；
- 使用原生FreeRTOS API，不编译CMSIS-RTOS封装。

这不是删除源码后形成的私人精简版，而是“配置裁剪 + 编译文件裁剪 + 链接垃圾回收”。外层保留完整官方内核，便于升级；固件只编译需要的部分。

实际编译：

| 文件 | 作用 |
|---|---|
| `tasks.c` | 任务创建、调度、延时、挂起和任务通知 |
| `list.c` | 调度器内部链表 |
| `queue.c` | 队列、信号量、互斥量基础 |
| `heap_4.c` | 支持合并空闲块的动态内存管理 |
| `portable/GCC/ARM_CM0/port.c` | Cortex-M0调度与SysTick适配 |
| `portable/GCC/ARM_CM0/portasm.c` | SVC、PendSV上下文切换 |
| `os_hooks.c` | malloc失败和任务栈溢出处理 |

当前没有编译 `timers.c`、`event_groups.c`、`stream_buffer.c`、`croutine.c` 和CMSIS-RTOS封装。

## 5. 关键配置

| 配置 | 当前值 | 说明 |
|---|---:|---|
| CPU clock | 48 MHz | 使用 `SystemCoreClock` |
| Tick rate | 1000 Hz | FreeRTOS时间粒度1 ms |
| Tick width | 32 bit | 约49.7天回绕一次 |
| 最大优先级数量 | 5 | 可用优先级0~4 |
| Idle task stack | 64 words | 256 bytes |
| LED task stack | 128 words | 512 bytes |
| BEEP task stack | 96 words | 384 bytes |
| FreeRTOS heap | 2048 bytes | `heap_4.c` 的 `ucHeap` |
| 动态分配 | 开启 | 任务栈和TCB从 `ucHeap` 分配 |
| 静态分配 | 关闭 | 不使用 `xTaskCreateStatic()` |
| 软件定时器 | 关闭 | 不创建Timer Service任务 |
| Task Notification | 开启 | 轻量任务同步 |
| Mutex | 开启 | 共享外设保护 |
| Tickless idle | 关闭 | 当前优先保证调试稳定性 |
| 栈溢出检查 | 等级2 | 检查任务栈边界 |
| malloc失败Hook | 开启 | 内核堆不足时停机 |

中断与时间基准：

- `SysTick`：FreeRTOS 1 ms Tick；
- `PendSV`：任务上下文切换；
- `SVC`：启动第一个任务；
- `TIM1`：HAL 1 ms Tick，因此 `HAL_Delay()` 不与FreeRTOS SysTick抢占同一个计时源。

任务代码中仍应使用 `vTaskDelay()`，因为它会阻塞当前任务并允许其他任务运行；`HAL_Delay()` 是轮询等待，不应该用来编写RTOS任务节奏。

## 6. Idle任务在哪里

Idle任务不是用户手写在 `User_tasks` 中的文件，所以看不到 `idle_task.c`。

它位于外层内核源码 `FreeRTOS/tasks.c`：

- `vTaskStartScheduler()` 启动调度器；
- 内部调用 `prvCreateIdleTasks()`；
- `prvCreateIdleTasks()` 创建入口为 `prvIdleTask()` 的Idle任务；
- 其任务名为 `IDLE`，优先级固定为 `tskIDLE_PRIORITY`，即0；
- 栈大小取 `configMINIMAL_STACK_SIZE`，当前为64 words，即256 bytes。

Idle任务负责在没有更高优先级Ready任务时占用CPU，并回收已删除任务的内存。当前 `configUSE_IDLE_HOOK=0`，因此不会调用 `vApplicationIdleHook()`；同时 `INCLUDE_vTaskDelete=0`，当前也没有任务删除后的内存回收需求。

LED任务处于 `vTaskDelay()`、BEEP任务完成后处于Suspend时，Idle任务就会运行。这是正常现象，不代表CPU异常空转。

## 7. RAM与FreeRTOS heap

STM32F042K6共有6144 bytes SRAM。当前Debug/Release的静态RAM结果为：

| 区域 | 字节数 | 内容 |
|---|---:|---|
| `.data` | 16 | 已初始化全局变量 |
| `.bss` | 2440 | 包含2048-byte `ucHeap`及内核全局变量 |
| C heap + MSP预留 | 1536 | 512-byte newlib heap + 1024-byte主栈 |
| 合计 | 3992 | SRAM占用64.97% |
| 链接后未占用 | 2152 | SRAM剩余35.03% |

这里有两个“剩余内存”概念：

- 2152 bytes：链接器看到的芯片SRAM空余；
- 约592 bytes：调度器创建LED、BEEP、Idle三个任务后，2048-byte FreeRTOS heap内部预计剩余。

当前编译结果中TCB大小为80 bytes，`heap_4` 每次分配还需要8-byte管理头。理论分配如下：

| 任务 | 栈实际分配 | TCB实际分配 | 合计 |
|---|---:|---:|---:|
| LED | 512 + 8 | 80 + 8 | 608 bytes |
| BEEP | 384 + 8 | 80 + 8 | 480 bytes |
| Idle | 256 + 8 | 80 + 8 | 352 bytes |
| 合计 |  |  | 1440 bytes |

由于当前 `ucHeap` 地址和8-byte对齐、末尾管理块会消耗一小部分，初始化后可分配空间约2032 bytes，因此三个任务创建后理论值约为592 bytes。最终值必须在目标板运行后读取：

```c
size_t current_free = xPortGetFreeHeapSize();
size_t minimum_free = xPortGetMinimumEverFreeHeapSize();
UBaseType_t stack_margin = uxTaskGetStackHighWaterMark(NULL);
```

新增任务时必须检查：

1. `xTaskCreate()` 是否等于 `pdPASS`；
2. 创建前后FreeRTOS heap变化；
3. `xPortGetMinimumEverFreeHeapSize()`；
4. 每个任务的Stack High Water Mark；
5. malloc failed hook和stack overflow hook；
6. 中断嵌套后的MSP是否仍小于预留1024 bytes。

不要使用外层 `FreeRTOS/FreeRTOSConfig.h`。那是参考配置，其中170 MHz和25 KB heap不适用于STM32F042。本工程通过Include Path使用本目录的配置。

## 8. 蜂鸣器任务为什么以前不运行

原理图中的BUZZER2是标注4 kHz的有源蜂鸣器，由PA1网络 `beef`、R8和S8550驱动。它不需要MCU产生4 kHz PWM；PA1输出低电平时三极管导通，蜂鸣器内部自行产生约4 kHz声音，PA1输出高电平时停止。

原实现有四个问题：

1. `user_tasks_init()` 只调用LED任务创建函数，没有调用蜂鸣器任务创建函数，这是任务不运行的决定性原因；
2. 仅把 `.c` 加进CMake不等于任务会执行；未被调用的创建函数还会被链接器垃圾回收，所以旧ELF中看不到蜂鸣器任务符号；
3. `User_tasks/user_beep_task.c#TODO...` 这种紧贴文件名的CMake注释可读性差，修改源码列表后也必须重新Configure，现已改成独立、无歧义的源码项；
4. 任务直接操作HAL并使用 `HAL_Delay(125)`：它实际等待125 ms，不是注释所写的12 us，而且RTOS任务节奏应使用 `vTaskDelay()`。

当前已经改为：

```text
beep_task.c
  -> bsp_beep_handler.c
      -> bsp_beep_driver.c
          -> HAL GPIO / PA1
```

任务已加入CMake并由 `task_manager_init()` 创建。烧录后预期现象是：蜂鸣器短鸣120 ms、静音180 ms，共三次，然后保持关闭并挂起该任务。

如果三次短鸣不断重新出现，不是任务在循环，而是MCU可能在不断复位，应同时观察LED灯效是否也从头开始，并测量NRST、3.3 V和PA1。如果PA1始终稳定高电平但仍持续轻响，再检查S8550管脚、焊接、漏电和蜂鸣器器件方向。

## 9. 状态检查和日志预留

代码不把多个关键操作压缩在一条 `return a() && b() && c()` 中。对可能失败的步骤，应保存状态、逐步判断，并把原始错误向上传递，以便确定失败层级和失败原因。

推荐结构：

```c
led_handler_status_t status = bsp_led_handler_commit();
if (HANDLER_OK != status)
{
    /* 日志预留：记录提交失败及status。 */
    return status;
}
```

不能只判断 `HANDLER_ERROR == status`，因为这样会漏掉Timeout、Resource、Parameter和NoMemory等错误。统一采用 `SUCCESS_STATUS != status` 检查所有非成功状态。

当前阶段已经删除 `Middlewares/Log`、`APP_LOG_INFO`、`APP_LOG_ERROR` 和对应CMake配置。代码只在关键错误分支保留“日志预留”注释，后续移植EasyLogger时再接入，不提前引入无输出通道的打印代码。

完整编码规则见 `docs/C_CODE_STYLE_GUIDE.md`。
