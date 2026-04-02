# Plan

Stage: ic_commands_sscom_test
Status: active
Priority: high
Source: Claude Code
Date: 2026-04-02 11:00:00
Depends-On: review_ic_commands_20260402_100000.md (pass)
Supersedes: none

## Goal

使用 SSCOM + com0com 虚拟串口对，对 IC Scan 和 Connect 按钮进行端到端测试，验证串口命令发送与响应处理全流程正确。

## Scope

- 临时将 `RetryTimeoutMs` 从 100 改为 10000（给手动操作留时间）
- 执行 SSCOM 测试
- 测试完成后将 `RetryTimeoutMs` 改回 100

## Non-Goals

- 不修改任何业务逻辑
- 不修改协议

## Files-In-Scope

- `src/serialmanager.h`（仅修改 RetryTimeoutMs 常量，测试前改大，测试后改回）

## Files-Frozen

- `src/tabs/configtab.h` / `src/tabs/configtab.cpp`
- `src/serialmanager.cpp`
- 其他所有文件

---

## 步骤一：临时放大超时

将 `src/serialmanager.h` 中：

```cpp
static constexpr int RetryTimeoutMs = 100;
```

改为：

```cpp
static constexpr int RetryTimeoutMs = 10000;
```

重新编译：`mingw32-make -C build_make_qt -j4`

---

## 测试环境

- com0com 虚拟串口对：COM11 ↔ COM12
- MotorDev 连接 COM11（115200，8N1）
- SSCOM 连接 COM12（115200，8N1），开启 HEX 显示和 HEX 发送

## CRC 计算脚本

```python
def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0x8005
            else:
                crc >>= 1
    return crc

def build_frame(seq: int, cmd: int, data: bytes) -> str:
    payload = bytes([seq, cmd, len(data)]) + data
    crc = crc16_modbus(payload)
    frame = bytes([0xAA, 0x55]) + payload + bytes([crc >> 8, crc & 0xFF])
    return ' '.join(f'{b:02X}' for b in frame)
```

---

## 步骤二：测试

### 测试一：I2C Scan

1. MotorDev 连接串口后，点击 IC 框的 **Scan** 按钮
2. SSCOM 收到帧，格式为：`AA 55 [SEQ] 07 01 02 [CRC_HI] [CRC_LO]`，记录 `[SEQ]`
3. 用脚本计算响应帧（模拟发现 2 个设备 0x18 和 0x20）：
   ```python
   build_frame(seq=<SEQ>, cmd=0x07, data=bytes([0x02, 0x18, 0x20]))
   ```
4. 在 SSCOM 中 HEX 发送计算出的帧

**预期结果：**
- 控制台输出 `I2C scan found 2 devices` 及两个地址
- Slave ID ComboBox 填充 "0X18"、"0X20"

### 测试二：IC Connect（成功）

1. Slave ID ComboBox 选择 "0X18"，点击 **Connect**
2. SSCOM 收到帧：`AA 55 [SEQ] 02 01 18 [CRC_HI] [CRC_LO]`，记录 `[SEQ]`
3. 用脚本计算成功响应帧（LEN=0）：
   ```python
   build_frame(seq=<SEQ>, cmd=0x02, data=bytes([]))
   ```
4. SSCOM 发送

**预期结果：**
- 控制台输出 `Motor IC address set to 0X18`

### 测试三：错误响应

1. 点击 Scan 或 Connect 后，用脚本计算错误帧：
   ```python
   build_frame(seq=<SEQ>, cmd=0x01, data=bytes([0x03]))
   ```
2. SSCOM 发送

**预期结果：**
- 控制台输出 `Error response: code=0X03 (Execution failed)`

### 测试四：按钮禁用

1. 断开串口，确认 Scan/Connect 按钮变灰不可点击
2. 重新连接，确认按钮恢复可用

---

## 步骤三：测试完成后恢复

将 `src/serialmanager.h` 中 `RetryTimeoutMs` 改回：

```cpp
static constexpr int RetryTimeoutMs = 100;
```

重新编译确认通过。

---

## Acceptance

1. **Scan 发送帧正确** — SSCOM 收到 `AA 55 [SEQ] 07 01 02 [CRC]`
2. **Scan 响应处理** — Slave ID ComboBox 填充正确地址列表
3. **Connect 发送帧正确** — SSCOM 收到 `AA 55 [SEQ] 02 01 18 [CRC]`
4. **Connect 成功响应** — 控制台输出 `Motor IC address set to 0X18`
5. **错误响应处理** — 控制台输出错误码和描述
6. **按钮禁用** — 未连接时 Scan/Connect 灰色不可点击
7. **RetryTimeoutMs 已恢复 100** — 编译通过

## Notes

- SEQ 由 SerialManager 自动递增，每次从 SSCOM 收到帧后先读取 SEQ，再计算响应帧
- 心跳帧（cmd=0x00）会定期出现在 SSCOM，忽略即可，或回复 `build_frame(seq, 0x00, bytes([]))`
