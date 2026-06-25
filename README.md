[中文](README_cn.md) | English

# MotionCal (BLE HID Edition)

A Bluetooth Low Energy (BLE HID) fork of [PaulStoffregen/MotionCal](https://github.com/PaulStoffregen/MotionCal). This version replaces serial communication with a custom HID protocol stack for wireless data exchange and magnetometer calibration.

## Build Environment

1. **Cross-compilation Toolchain**

   On Linux (e.g., Ubuntu), install the MinGW-w64 cross-compilation toolchain:

   ```bash
   sudo apt update
   sudo apt install build-essential gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64
   ```

2. **Build wxWidgets from Source (Required)**

   Due to cross-architecture and C++ ABI differences, wxWidgets must be statically compiled for MinGW:

   1. Download wxWidgets 3.2+ source from the [official site](https://www.wxwidgets.org/) and extract.
   2. Configure and build:
      ```bash
      mkdir build-mingw64 && cd build-mingw64
      ../configure --host=x86_64-w64-mingw32 --disable-shared --prefix=$PWD/install
      make -j$(nproc) && make install
      ```
   3. After building, `wx-config` is located at `install/bin/wx-config`.

3. **Dependencies**

   HID support is integrated directly via `third_party/hidapi` source code. The Makefile automatically links Windows system libraries (`-lhid -lsetupapi -lole32`) through MinGW.

## Build

Run `make` from the project root (the directory containing the Makefile). Pass environment variables for your target platform:

```bash
make OS=WINDOWS MINGW_TOOLCHAIN=x86_64-w64-mingw32 WXCONFIG=/path/to/wx-config
```

To clean and rebuild from scratch:

```bash
make clean
```

A successful build produces an executable (e.g., `MotionCal.exe`) in the current directory.

## Logging

Debug logging helps trace HID packet transmission and program state.

- **Enable log output**: Logging is disabled by default. To enable it, open `debuglog.c` and change `log_file = NULL;` to `log_file = fopen(log_path, "a");` inside `debuglog_init()`.
- **Verbose mode**: Create an empty file named `MotionCal.debug` in the same directory as `MotionCal.exe`. The program will automatically enable verbose mode (`verbose_enabled = 1`) on startup, producing finer-grained communication logs. Log output is written to `MotionCal.log`.

## HID Data Format

This version replaces the serial port with standard HID Report messages. The format is as follows:

### 1. Sensor Data Report (Input Report ID 3 or 6)

Contains raw 9-axis sensor samples (pre-fusion), little-endian.

| Bytes      | Field                         |
|------------|-------------------------------|
| 0          | Report ID (3 or 6)            |
| 1–4        | Hardware timestamp (uint32)   |
| 5–16       | Accelerometer X/Y/Z (3×Float32, unit: m/s²) |
| 17–28      | Gyroscope X/Y/Z (3×Float32, unit: dps)      |
| 29–40      | Magnetometer X/Y/Z (3×Float32, unit: µT)    |

These values are then converted to int16 counts using specific scaling factors for the original calibration algorithm.

### 2. Acquisition Control (Output Report ID 1 / CMD 1)

Starts or stops data streaming from the device.

- **Format**: `[Report ID=1] [Command=1] [Mode (1 byte)]`
  - `Mode = 0`: Pause reporting (prevents BLE congestion when writing large calibration matrices).
  - `Mode = 1`: Resume reporting.

### 3. Write Calibration Parameters (Output Report ID 1 / CMD 8)

Writes magnetometer calibration parameters (hard-iron, soft-iron matrix) to the device.

- **Format**: `[Report ID=1] [Command=8] [Index (1 byte)] [Payload]`
  - `Index = 0`: Payload is a 1-byte enable flag (`0x00` / `0x01`).
  - `Index = 1–12`: Payload is a 4-byte Float32 calibration value.

### 4. Read Parameters (Output Report ID 1 / CMD 12)

Reads back bytes from a device EEPROM offset to verify writes.

- **Request**: `[Report ID=1] [Command=12] [Offset (2 bytes, little-endian)] [Len (1 byte)]`
- **Response (Input Report ID 4)**: `[Report ID=4] [Offset (2 bytes, little-endian)] [Len (1 byte)] [Payload]`

## Porting Guide

To adapt this project to your own hardware firmware, focus on the core parsing and packing functions in `blehid.c`:

1. **Modify sensor report parsing**
   - Locate `parse_report3_payload(const unsigned char *buf, int len, int report_id)`.
   - This function parses the byte array into `Acc`, `Gyro`, `Mag` values and feeds them to the calibration core `raw_data()`. Adjust endianness and scaling factors to match your device's output.

2. **Modify command format**
   - Locate `blehid_write_mag_cal(unsigned char index, float value)`.
   - The current logic sends `Index` + 4-byte `Float` to CMD 8. If your device requires different command bytes or data structures (e.g., fixed-point), modify the `data[]` packing here.

3. **Change Report IDs**
   - Search `blehid.c` for conditions like `buf[0] == 3` or `buf[0] == 6` and update them as needed.

## License

Based on [PaulStoffregen/MotionCal](https://github.com/PaulStoffregen/MotionCal). See the original project for license details.
