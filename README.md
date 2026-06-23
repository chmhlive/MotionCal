# MotionCal (BLE HID Edition)

本项目是基于 PaulStoffregen/MotionCal 开发的低功耗蓝牙（BLE HID）改造版本。通过定制的 HID 协议栈实现了与磁力计传感器的数据交互与无线校准。

## 编译环境搭建

1. **基础编译工具链**：
   在 Linux（如 Ubuntu）下执行交叉编译需要安装对应架构的工具链：
   ```bash
   sudo apt update
   sudo apt install build-essential gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64
   ```
2. **源码编译 wxWidgets (必做)**：
   由于跨架构和 C++ ABI 差异，必须针对 MinGW 静态编译 wxWidgets：
   1. 从官网下载 wxWidgets 3.2+ 源码并解压。
   2. 在其根目录下执行编译配置与安装：
      ```bash
      mkdir build-mingw64 && cd build-mingw64
      ../configure --host=x86_64-w64-mingw32 --disable-shared --prefix=$PWD/install
      make -j$(nproc) && make install
      ```
   3. 编译完成后，`wx-config` 脚本位于 `install/bin/wx-config`。
3. **依赖库**：项目通过 `third_party/hidapi` 源码直接集成了对 HID 的支持，编译时 Makefile 会自动通过 MinGW 链接 Windows 底层库（`-lhid -lsetupapi -lole32`）。

## 编译方法

在项目根目录（即包含 Makefile 的目录）执行标准的 `make` 命令即可进行编译。
可以根据目标平台传递相应的环境变量，例如：
```bash
make OS=WINDOWS MINGW_TOOLCHAIN=x86_64-w64-mingw32 WXCONFIG=/path/to/wx-config
```
若需清理后彻底重新编译：
```bash
make clean
```
编译成功后，将在当前目录下生成可执行文件（例如 `MotionCal.exe`）。

## 日志开关

调试日志功能可以帮助追踪 HID 报文收发及程序运行状态。

- **开启日志生成**：默认情况下日志写入被关闭。如需开启，请打开 `debuglog.c`，在 `debuglog_init()` 函数中将 `log_file = NULL;` 修改为 `log_file = fopen(log_path, "a");`。
- **Verbose（详细信息）模式**：若在 `MotionCal.exe` 所在的同级目录下新建一个名为 `MotionCal.debug` 的空文件，程序启动时会自动开启详细模式（`verbose_enabled = 1`），输出更细颗粒度的通信信息。日志文件将输出为 `MotionCal.log`。

## HID 数据的格式

本版本舍弃了串口，完全采用标准的 HID Report 报文通信。格式约定如下：

1. **传感器上报数据 (Input Report ID 3 或 6)**
   - 包含传感器融合前的 9 轴底层原始采样，小端序。
   - **格式约定**：
     - `Byte 0`: Report ID (3 或 6)
     - `Byte 1~4`: 硬件时间戳 (uint32)
     - `Byte 5~16`: 加速度计 Acc X/Y/Z (3×Float32, 单位: m/s²)
     - `Byte 17~28`: 陀螺仪 Gyro X/Y/Z (3×Float32, 单位: dps)
     - `Byte 29~40`: 磁力计 Mag X/Y/Z (3×Float32, 单位: µT)
   - 此数据随后会被按照特定的系数转换为原版需要的 int16 型计数值（Count）。

2. **采集控制命令 (Output Report ID 1 / CMD 1)**
   - 用于主机启停底层设备的数据流发送。
   - **格式**：`[Report ID=1] [Command=1] [Mode(1字节)]`
     - `Mode = 0`：暂停上报（防止下发大批量磁校准矩阵时造成蓝牙拥堵丢包）。
     - `Mode = 1`：恢复上报。

3. **写参数配置命令 (Output Report ID 1 / CMD 8)**
   - 向设备写入磁校准参数（硬铁、软铁矩阵）。
   - **格式**：`[Report ID=1] [Command=8] [参数索引 Index(1字节)] [Payload字节流]`
     - 当 `Index=0` 时，Payload 为 `1` 字节的使能开关 `0x00/0x01`。
     - 当 `Index=1~12` 时，Payload 为 `4` 字节的 Float32 校准值。

4. **读参数命令 (Output Report ID 1 / CMD 12)**
   - 主机用于校验写入是否成功，从设备 EEPROM 偏移行中读回特定长度的字节。
   - **请求格式**：`[Report ID=1] [Command=12] [Offset(2字节小端)] [Len(1字节)]`
   - **应答格式 (Input Report ID 4)**：`[Report ID=4] [Offset(2字节小端)] [Len(1字节)] [Payload字节流]`

## 移植与二次开发指南

如果你需要将此项目适配自己的硬件固件，请关注 `blehid.c` 文件中的几个核心解析与打包函数：

1. **修改传感器报文解析方式**：
   - 定位到 `parse_report3_payload(const unsigned char *buf, int len, int report_id)` 函数。
   - 此函数负责将字节数组解析为 `Acc`、`Gyro`、`Mag`，再传入算法核心 `raw_data()`。你需要根据自身下位机的数据组装端序（大/小端）及缩放倍率修改此处。
2. **修改命令下发格式**：
   - 定位到 `blehid_write_mag_cal(unsigned char index, float value)`。
   - 现有的逻辑是发送 `Index` + 4 字节的 `Float` 给 CMD 8。若你的下位机需要不同的命令字或数据结构（如定点数），请在此处打包数组 `data[]`。
3. **更改 Report ID**：
   - 直接搜索 `blehid.c` 中诸如 `buf[0] == 3` 或 `buf[0] == 6` 的判断条件并修改即可。
