# Report

Stage: serial_core
Status: implemented
Source: Codex
Date: 2026-04-01 23:20:00
Based-On: plan_serial_core_20260401_230000.md

## Completed

- Added `FrameParser` with:
  - control frame encoding
  - CRC16-MODBUS calculation
  - byte-wise parser state machine
  - control frame decode with CRC validation
  - stream frame decode with LEN/XOR validation
- Added `SerialManager` with:
  - worker-thread initialization via `moveToThread`
  - `QSerialPort` creation inside the worker thread
  - `availablePorts / openPort / closePort / sendCommand` public interfaces
  - single-command pending queue
  - 100ms retry timer with max 2 retries
  - 1-second heartbeat timer with disconnect on 3 missed heartbeats
  - parsed control-frame and stream-frame signal dispatch
- Updated `CMakeLists.txt` to include the new communication-layer files.

## Files-Changed

- `src/frameparser.h`
- `src/frameparser.cpp`
- `src/serialmanager.h`
- `src/serialmanager.cpp`
- `CMakeLists.txt`

## Verification

- `Get-Content -Path protocol.md -Encoding utf8`: re-checked protocol requirements before implementation
- `D:\Qt\Tools\CMake_64\bin\cmake.exe -S . -B build_make_qt -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="D:\Qt\6.11.0\mingw_64" -DQt6_DIR="D:\Qt\6.11.0\mingw_64\lib\cmake\Qt6" -DCMAKE_CXX_COMPILER="D:\Qt\Tools\mingw1310_64\bin\g++.exe" -DCMAKE_MAKE_PROGRAM="D:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe"`: passed
- `D:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe -C build_make_qt -j4`: passed
- Verified rebuilt binary exists at `build_make_qt/MotorDev.exe`
- Verified control-frame example payload `[0x01, 0x00, 0x00]` produces CRC `0x0020`, so `encodeControlFrame(0x01, 0x00, {})` corresponds to `AA 55 01 00 00 00 20`

## Risks

- No runtime serial-port loopback or device-backed validation was performed in this round; current verification is compile-level plus protocol-path inspection.
- `SerialManager` is not yet connected to any UI or higher-level command wrapper, which is expected for this stage.

## Open-Questions

- None.
