> 文档编号：05

# CMake固件构建、版本命名与OpenOCD烧录

## 1. 结论

版本化固件不是只能使用一次的“固定旧固件”。修复和新功能都保存在工程源码中；只要当前源码目录不被替换，以后执行CMake构建，就会把最新代码重新编译成固件。当前阶段的版本标签是 `STAGE3_R1`。

```text
当前源码
  -> CMake配置
  -> GCC编译为.obj
  -> Linker链接为M0_Init.elf
  -> CMake Post-Build自动生成带版本名的ELF/HEX/BIN
```

`build/` 目录是构建产物，不是源码的唯一保存位置。即使删除整个 `build/cmake/Release`，也能从 `Core`、`BSP`、`User_tasks`、`Middlewares` 和顶层 `CMakeLists.txt` 重新生成固件。

## 2. 版本信息的唯一维护位置

顶层 `CMakeLists.txt` 中有：

```cmake
set(FIRMWARE_BUILD_TAG "STAGE3_R1")
```

这个值同时用于：

- MCU二进制HELLO响应中的固件标签：`STAGE3_R1`；
- 自动生成 `M0_Init_STAGE3_R1.elf`；
- 自动生成 `M0_Init_STAGE3_R1.hex`；
- 自动生成 `M0_Init_STAGE3_R1.bin`。

通信任务不再单独写死版本，而是使用CMake传入的 `FIRMWARE_BUILD_TAG`。因此文件名和固件内部版本不会分别维护、相互写错。

版本标签表示“固件接口/行为版本”，不是每按一次Build都必须增加。普通代码修改后可以继续构建 `STAGE3_R1`；当通信协议、关键修复或阶段发生变化时，再有意识地改为例如 `STAGE3_R2` 或 `STAGE4_R1`。

## 3. 日常Release构建

在Windows PowerShell中逐条执行：

```powershell
cd D:\M0\five_data\qyan_Tacglove
```

```powershell
cmake --preset Release
```

```powershell
cmake --build --preset Release
```

成功时最后会看到：

```text
Linking C executable M0_Init.elf; Generating M0_Init_STAGE3_R1.elf/.hex/.bin
```

输出目录：

```text
D:\M0\five_data\qyan_Tacglove\build\cmake\Release
```

每次链接成功后自动生成：

| 文件 | 用途 |
| --- | --- |
| `M0_Init.elf` | CMake目标的标准名称，始终代表本次最新构建 |
| `M0_Init_STAGE3_R1.elf` | 内容相同的版本化ELF，推荐烧录和归档 |
| `M0_Init_STAGE3_R1.hex` | Intel HEX，包含Flash地址，可直接烧录 |
| `M0_Init_STAGE3_R1.bin` | 裸二进制，烧录时需要地址 `0x08000000` |
| `M0_Init.map` | 链接映射，用于检查符号和内存占用 |

## 4. 什么时候只Build，什么时候重新Configure

只修改已经加入工程的 `.c/.h`：

```powershell
cmake --build --preset Release
```

修改以下内容后，应先Configure再Build：

- `CMakeLists.txt`；
- 新增或删除 `.c` 文件；
- 修改Include目录；
- 修改编译宏或固件版本标签；
- CubeMX重新生成工程；
- 修改工具链或链接脚本。

```powershell
cmake --preset Release
cmake --build --preset Release
```

如果怀疑旧对象或缓存没有更新，可以执行一次干净构建：

```powershell
cmake --build --preset Release --clean-first
```

该命令只清理Release构建产物，然后从当前源码重新编译，不会删除工程源码。

## 5. 使用OpenOCD烧录最新版本化ELF

### 推荐：一条命令构建并烧录Release

工程只在Release preset中提供 `flash` 目标。该目标依赖当前固件目标：源码有变化时先自动构建，然后直接烧录本次Release目录中的标准ELF，不需要人工选择Debug/Release路径。

```powershell
cd D:\M0\five_data\qyan_Tacglove
cmake --preset Release
cmake --build --preset Release --target flash
```

不要使用 `--preset Debug --target flash`；Debug配置故意不创建该烧录目标。

### 手工指定版本化Release ELF

```powershell
cd D:\M0\five_data\qyan_Tacglove
```

```powershell
openocd -f "openocd.cfg" -c "gdb port disabled" -c "init; reset init" -c "program build/cmake/Release/M0_Init_STAGE3_R1.elf verify reset exit"
```

烧录前一定先确认Build成功。不要从其他工程目录复制一个同名ELF，也不要继续烧录 `build/cmake/Debug` 下以前生成的文件。

OpenOCD也可以直接烧录HEX：

```powershell
openocd -f "openocd.cfg" -c "gdb port disabled" -c "init; reset init" -c "program build/cmake/Release/M0_Init_STAGE3_R1.hex verify reset exit"
```

HEX和ELF都包含地址。只有BIN需要显式指定Flash起始地址：

```powershell
openocd -f "openocd.cfg" -c "gdb port disabled" -c "init; reset init" -c "program build/cmake/Release/M0_Init_STAGE3_R1.bin 0x08000000 verify reset exit"
```

## 6. 确认ELF内部版本

可以在烧录前检查ELF里包含的版本标签：

```powershell
arm-none-eabi-strings build/cmake/Release/M0_Init_STAGE3_R1.elf | Select-String STAGE3_R1
```

期望：

```text
STAGE3_R1
```

也可以记录固件哈希，确认归档或传输后文件未变化：

```powershell
Get-FileHash build/cmake/Release/M0_Init_STAGE3_R1.elf -Algorithm SHA256
```

只要源码发生变化，重新构建后的哈希通常也会变化；文件名仍可保持 `STAGE3_R1`，直到你决定发布新的接口/行为版本。

## 7. Debug与Release的区别

两个Preset现在都会自动生成版本化ELF/HEX/BIN，但板卡日常运行和发布优先使用Release：

- Release使用 `-Os`，Flash占用更小；
- 当前SK6805软件时序按48 MHz Release构建标定；
- Debug包含大量调试符号，磁盘上的ELF会明显更大；
- ELF文件在磁盘上的大小不等于烧入Flash的大小，最终以Build输出的Memory Region统计为准。

如果使用GDB单步调试，可以构建Debug；完成调试后仍建议重新构建并烧录Release。

### 为什么阶段1曾出现“普通ELF失败、R3 ELF成功”

文件是否带R3不是运行差异的来源；必须同时看所在目录：

```text
build/cmake/Debug/M0_Init.elf                 Debug，-O0
build/cmake/Debug/M0_Init_STAGE3_R1.elf       与上面的Debug ELF内容相同

build/cmake/Release/M0_Init.elf               Release，-Os
build/cmake/Release/M0_Init_STAGE3_R1.elf     与上面的Release ELF内容相同
```

阶段1实测失败的是Debug标准ELF，实测1000/1000通过的是Release R3 ELF。这说明差异来自Build Type，而不是文件名。当前通信任务栈已调整为320字节，但SK6805软件时序仍只按48 MHz Release构建标定，因此板上验收和日常烧录仍统一使用Release，Debug只用于连接GDB定位问题。

## 8. CubeMX重新生成后的固定流程

1. 在CubeMX内完成外设设置并Generate Code；
2. 核对每次交付给出的“CubeMX同步清单”；
3. 确认自定义 `BSP`、`User_tasks`、`Middlewares/FreeRTOS` 没有被删除；
4. 确认顶层 `CMakeLists.txt` 中仍有 `FIRMWARE_BUILD_TAG` 和Post-Build生成命令；
5. 执行 `cmake --preset Release`；
6. 执行 `cmake --build --preset Release`；
7. 检查Flash/RAM占用并烧录版本化ELF；
8. 运行对应的PC/WSL测试脚本。

本次“自动生成版本化固件”只修改了CMake构建逻辑，没有新增或变更CubeMX外设配置。

## 9. WSL串口节点与固件构建无关

WSL重启、CP210x拔插或usbipd重新连接时，`/dev/ttyUSB0` 可能暂时不存在。这不表示固件构建失败。先在Windows确认usbipd状态为Attached，再在WSL执行：

```bash
ls -l /dev/ttyUSB*
```

节点出现后再运行测试。阶段1的1000/1000 PASS已经证明RX DMA、TX DMA恢复机制和WSL串口链路能够正常往返；阶段2必须改用二进制协议测试脚本，旧的ASCII脚本超时是预期现象。
