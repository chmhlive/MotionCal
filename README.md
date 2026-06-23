# MotionCal (BLE HID Edition)

本项目是基于 PaulStoffregen/MotionCal 开发的低功耗蓝牙（BLE HID）改造版本。通过定制的 HID 协议栈实现了与磁力计传感器的数据交互与无线校准。

## 编译环境搭建

本项目推荐在 Linux（或 WSL）系统下，使用交叉编译工具链生成 Windows 可执行文件。

1. **安装 MinGW-w64**：确保环境中拥有 `x86_64-w64-mingw32-gcc` 及 `g++` 工具链。
2. **准备 wxWidgets**：
   - 使用 MinGW 工具链对 wxWidgets 进行**静态编译**。
   - 确保存在可执行的 `wx-config`。
3. **依赖库**：项目需要 `hidapi` 以及 Windows 标准的 `SetupAPI`、`Ole32`、`uuid`。

## 编译方法

在本项目根目录执行我们提供的脚本即可进行增量编译。
如果你的 `wxWidgets` 安装路径与默认不符，可以通过 `WXCONFIG` 环境变量进行指定：
```bash
WXCONFIG=/path/to/your/wx-config ./build_windows.sh
```
若需清理后彻底重新编译，可带上参数：
```bash
./build_windows.sh clean
```
编译成功后，将在当前目录下生成 `MotionCal.exe`，构建日志存放于 `/tmp/motion_cal-build.log`。

## 日志开关

调试日志功能可以帮助追踪 HID 报文收发及程序运行状态。

- **开启日志生成**：默认情况下日志写入被关闭。如需开启，请打开 `debuglog.c`，在 `debuglog_init()` 函数中将 `log_file = NULL;` 修改为 `log_file = fopen(log_path, "a");`。
- **Verbose（详细信息）模式**：若在 `MotionCal.exe` 所在的同级目录下新建一个名为 `MotionCal.debug` 的空文件，程序启动时会自动开启详细模式（`verbose_enabled = 1`），输出更细颗粒度的通信信息。日志文件将输出为 `MotionCal.log`。

## HID 数据的格式

本版本舍弃了串口，完全采用标准的 HID Report 报文通信。格式约定如下：

1. **传感器上报数据 (Report ID 3 或 6)**
   - **格式**：首字节为 ID，其后跟随打包好的加速度计（Acc）、陀螺仪（Gyro）和磁力计（Mag）原始数据。
   - 长度：通常为 41 或 43 字节以上。
2. **写参数命令 (CMD 8)**
   - 用于主机向设备下发配置或校准数据。
   - **格式**：`[Report ID=8] [参数索引 Index(1字节)] [浮点数值(4字节，小端序)]`
3. **读参数命令 (CMD 12)**
   - 用于主机读取设备中储存的数据（如读回确认校验）。
   - 搭配偏移地址及长度使用。
4. **采集控制命令 (CMD 1)**
   - **格式**：`[Report ID=1] [模式 Mode(1字节)]`
   - `Mode = 0`：暂停传感器数据采集（下发大批量配置前必须执行，防止信道拥堵）。
   - `Mode = 1`：恢复传感器数据上报。

## 他人使用时：如何修改 HID 的数据帧？

如果你需要将此项目适配自己的硬件，请关注 `blehid.c` 文件中的几个核心解析与打包函数：

1. **修改传感器报文解析方式**：
   - 定位到 `parse_report3_payload(const unsigned char *buf, int len, int report_id)` 函数。
   - 此函数负责将字节数组解析为 `Acc`、`Gyro`、`Mag`，再传入算法核心 `raw_data()`。你需要根据自身下位机的数据组装端序（大/小端）及缩放倍率修改此处。
2. **修改命令下发格式**：
   - 定位到 `blehid_write_mag_cal(unsigned char index, float value)`。
   - 现有的逻辑是发送 `Index` + 4 字节的 `Float` 给 CMD 8。若你的下位机需要不同的命令字或数据结构（如定点数），请在此处打包数组 `data[]`。
3. **更改 Report ID**：
   - 直接搜索 `blehid.c` 中诸如 `buf[0] == 3` 或 `buf[0] == 6` 的判断条件并修改即可。
