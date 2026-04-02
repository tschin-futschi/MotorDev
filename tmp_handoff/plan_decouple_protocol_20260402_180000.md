# Plan

Stage: decouple_protocol
Status: active
Priority: medium
Source: Claude Code
Date: 2026-04-02 18:00:00
Depends-On: review_register_rwtab_20260402_170000.md
Supersedes: none

## Goal

将通信协议字节帧的构建与解析从 UI Tab 类中抽离到独立的协议层；
消除 configtab.cpp 中散落的硬编码颜色值。
完成后换协议格式只需改协议层，UI 文件不变。

## Scope

1. 新建 `src/protocol/motor_protocol.h / .cpp`，实现协议编解码层
2. 修改 `src/tabs/registerrwtab.cpp`，改用协议层，不再内联构建字节帧
3. 修改 `src/tabs/configtab.cpp`，改用协议层，消除硬编码颜色
4. 修改 `src/ui/style_constants.h`，补充缺失颜色常量
5. 修改 `CMakeLists.txt`，加入新文件

## Non-Goals

- `RegisterTable::saveConfig / loadConfig` 的职责分离（不在本次范围）
- 队列管理逻辑的提取
- 任何功能改动：本次纯重构，行为必须与之前完全一致

## Files-In-Scope

- `src/protocol/motor_protocol.h`（新建）
- `src/protocol/motor_protocol.cpp`（新建）
- `src/tabs/registerrwtab.cpp`
- `src/tabs/configtab.cpp`
- `src/ui/style_constants.h`
- `CMakeLists.txt`

## Files-Frozen

- `src/tabs/registerrwtab.h`（接口不变）
- `src/tabs/configtab.h`（接口不变）
- `src/widgets/registertable.h / .cpp`
- `src/serialmanager.h / .cpp`
- `src/mainwindow.h / .cpp`
- 所有其他文件

## Constraints

### 协议层接口约束

`motor_protocol.h` 须暴露以下内容，放在 `namespace MotorProtocol` 内：

**命令码常量**（目前散落在各 Tab 的匿名 namespace 中，统一移至此处）：
- `CmdErrorResponse = 0x01`
- `CmdSetMotorIcAddr = 0x02`
- `CmdI2cBusScan = 0x07`
- `CmdReadRegister = 0x20`
- `CmdWriteRegister = 0x21`

**编码函数**（返回帧 payload，由调用方传给 SerialManager::sendCommand）：
- `QByteArray encodeReadRegister(quint16 addr)`
- `QByteArray encodeWriteRegister(quint16 addr, quint16 value)`
- `QByteArray encodeI2cBusScan()`
- `QByteArray encodeSetMotorIcAddr(uint8_t addr)`

**解码函数**（从响应 payload 中提取业务数据）：
- `bool decodeReadRegisterResponse(const QByteArray &data, quint16 *valueOut)`
- `QList<uint8_t> decodeI2cScanResponse(const QByteArray &data)`
- `uint8_t decodeErrorCode(const QByteArray &data)`

`MotorProtocol` 只做编解码，**不持有状态，不依赖 Qt Widget**。

### 颜色常量约束

以下颜色目前硬编码在 `configtab.cpp` 中，需提取至 `style_constants.h` 的 `namespace Color`：

| 变量名 | 值 | 用途 |
|---|---|---|
| `PrimaryButtonHover` | `#D5E8C4` | 主按钮 hover 背景 |
| `DisabledBackground` | `#E6E6E6` | 禁用按钮背景 |
| `DisabledBorder` | `#C9C9C9` | 禁用按钮边框 |
| `DisabledText` | `#9A9A9A` | 禁用按钮文字 |
| `PanelHoverBackground` | `#f5f5f2` | 面板 hover 背景 |
| `SecondaryButtonHover` | `#f0f0f0` | 次级按钮 hover 背景 |
| `SecondaryButtonPressed` | `#e0e0e0` | 次级按钮按下背景 |
| `PanelShadow` | `QColor(44,44,42,38)` | 面板阴影颜色 |

注：`makePrimaryButton` 中 pressed 背景 `#C0DD97` 与已有 `BorderGreen("#c0dd97")` 相同，直接引用即可，无需新增。

## Acceptance

- `registerrwtab.cpp` 中不再出现字面量 `0x20`、`0x21`、`0x01`
- `configtab.cpp` 中不再出现本地定义的 `CmdSetMotorIcAddr`、`CmdErrorResponse`、`CmdI2cBusScan`、`I2cBusIndex` 常量
- `configtab.cpp` 中不再出现以下硬编码颜色：`#D5E8C4`、`#E6E6E6`、`#C9C9C9`、`#9A9A9A`、`#f5f5f2`、`#f0f0f0`、`#e0e0e0`、`QColor(44, 44, 42, 38)`
- 编译通过，无新警告
- 运行行为与重构前完全一致（读寄存器、写寄存器、I2C 扫描、IC 连接均正常）

## Notes

- 本次是纯重构，禁止借机修改功能逻辑
- 如发现 protocol.md 中命令码定义与代码实际值有出入，需在 report 中标注，不得自行调整
