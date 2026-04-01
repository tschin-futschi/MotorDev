# Plan

Stage: config_logic
Status: active
Priority: high
Source: Claude Code
Date: 2026-04-02 00:00:00
Depends-On: review_serial_core_20260401_234000.md (pass)
Supersedes: none

## Goal

将 ConfigTab 的 Serial Group 和 IC Group 接入业务逻辑：实现 COM 口扫描与连接/断开，以及 IC 型号和 Slave ID 的全局状态管理。

## Scope

- 新建 `src/devicecontext.h` / `src/devicecontext.cpp` — 全局设备上下文（IC 型号 + Slave ID）
- 修改 `src/tabs/configtab.h` / `src/tabs/configtab.cpp` — 实现 connectSignals()，接入 SerialManager 和 DeviceContext
- 修改 `src/mainwindow.h` / `src/mainwindow.cpp` — 创建 SerialManager 和 DeviceContext 实例，传递给 ConfigTab
- 修改 `CMakeLists.txt` — 添加新文件

## Non-Goals

- 不实现 PMIC 配置逻辑（等待 STM32 固件支持）
- 不实现 Config File 读写逻辑
- 不实现 Slave ID 的 `0x02` 命令下发（仅存储，下发逻辑留给后续阶段在连接建立后自动执行）
- 不修改 SerialManager / FrameParser 的已有代码
- 不修改其他 Tab（RegisterRwTab / FwFlashTab / OscilloscopTab）

## Files-In-Scope

- `src/devicecontext.h` (新建)
- `src/devicecontext.cpp` (新建)
- `src/tabs/configtab.h` (修改)
- `src/tabs/configtab.cpp` (修改)
- `src/mainwindow.h` (修改)
- `src/mainwindow.cpp` (修改)
- `CMakeLists.txt` (添加新文件条目)

## Files-Frozen

- `src/frameparser.h` / `src/frameparser.cpp`
- `src/serialmanager.h` / `src/serialmanager.cpp`
- `src/tabs/registerrwtab.h` / `src/tabs/registerrwtab.cpp`
- `src/tabs/fwflashtab.h` / `src/tabs/fwflashtab.cpp`
- `src/tabs/oscilloscoptab.h` / `src/tabs/oscilloscoptab.cpp`
- `src/widgets/*.h` / `src/widgets/*.cpp`
- `src/ui/style_constants.h`
- `protocol.md`
- `design_spec.md`
- `CLAUDE.md`

## Constraints

- **UI 与业务逻辑分离**：ConfigTab 只负责将控件信号转发给 SerialManager / DeviceContext，不包含业务判断逻辑
- DeviceContext 必须提供信号，以便其他模块响应 IC 型号或 Slave ID 变化
- SerialManager 已在工作线程，ConfigTab 通过信号槽调用（Qt 自动排队连接）
- ConfigTab 需要持有 SerialManager 和 DeviceContext 的指针，通过构造函数或 setter 注入

---

## 详细设计

### 1. DeviceContext

全局设备上下文，存储当前 IC 型号和 Slave ID。生命周期由 MainWindow 管理，各模块通过指针访问。

#### 1.1 IC 型号枚举

```cpp
enum class MotorIcType {
    AW86006,
    DW9786,
    DW9788
};
```

#### 1.2 公共接口

```cpp
class DeviceContext : public QObject {
    Q_OBJECT

public:
    explicit DeviceContext(QObject *parent = nullptr);

    MotorIcType icType() const;
    void setIcType(MotorIcType type);

    // Slave ID：7-bit I2C 地址（0x01~0x7F）
    uint8_t slaveId() const;
    void setSlaveId(uint8_t id);

signals:
    void icTypeChanged(MotorIcType type);
    void slaveIdChanged(uint8_t id);

private:
    MotorIcType m_icType = MotorIcType::AW86006;
    uint8_t m_slaveId = 0x00;
};
```

#### 1.3 说明

- DeviceContext 运行在主线程，不需要 moveToThread
- 默认 IC 型号为 AW86006（与 UI 中 combo 默认选项一致）
- 默认 Slave ID 为 0x00（未设置）
- setter 仅在值变化时发射信号

### 2. ConfigTab 改动

#### 2.1 构造函数变更

```cpp
// 新增依赖注入
explicit ConfigTab(SerialManager *serialManager, DeviceContext *deviceContext, QWidget *parent = nullptr);
```

ConfigTab 持有 SerialManager 和 DeviceContext 的指针（不拥有所有权）。

#### 2.2 connectSignals() 实现

**Serial Group：**

- **Scan 按钮点击**：调用 `SerialManager::availablePorts()` 获取 COM 口列表，更新 m_portCombo 的选项
- **Connect 按钮点击**：
  - 若当前未连接 → 从 m_portCombo 和 m_baudRateCombo 获取参数，调用 SerialManager::openPort
  - 若当前已连接 → 调用 SerialManager::closePort
- **SerialManager::connected 信号**：
  - 按钮文字变为 "Disconnect"
  - 禁用 m_portCombo、m_baudRateCombo、m_scanButton（防止连接中修改）
- **SerialManager::disconnected 信号**：
  - 按钮文字恢复 "Connect"
  - 启用 m_portCombo、m_baudRateCombo、m_scanButton
- **SerialManager::errorOccurred 信号**：
  - 本阶段可简单处理，如 qWarning 输出，或在状态栏显示（如果 StatusBar 可用）

**IC Group：**

- **IC Combo 选项变更**：将 combo index 映射为 MotorIcType 枚举，调用 DeviceContext::setIcType
- **Slave ID 输入变更**：解析 hex 字符串为 uint8_t，调用 DeviceContext::setSlaveId
  - 使用 editingFinished 信号（用户按回车或焦点离开时触发），避免输入中途频繁更新

### 3. MainWindow 改动

- 在 MainWindow 中创建 SerialManager 和 DeviceContext 实例（成员变量）
- 将两者的指针传递给 ConfigTab 构造函数
- DeviceContext 由 MainWindow 直接持有（主线程对象）
- SerialManager 已自行管理线程，MainWindow 只负责创建和销毁

---

## Acceptance

1. **编译通过** — `mingw32-make -C build_make_qt -j4` 成功
2. **DeviceContext 接口完整** — icType/setIcType/slaveId/setSlaveId + 两个变更信号
3. **DeviceContext 信号正确** — 仅在值变化时发射
4. **Scan 按钮功能** — 点击后 m_portCombo 更新为当前可用 COM 口列表
5. **Connect 按钮功能** — 未连接时点击发起连接；已连接时点击断开
6. **连接状态 UI 反馈** — 连接后按钮变 "Disconnect"，禁用端口/波特率/扫描；断开后恢复
7. **IC Combo 联动** — 选择变更时 DeviceContext::icType 同步更新
8. **Slave ID 联动** — 输入确认后 DeviceContext::slaveId 同步更新
9. **UI 与逻辑分离** — ConfigTab::connectSignals() 只做信号槽连接和简单的 UI 状态切换，不包含通信协议细节
10. **依赖注入** — ConfigTab 通过构造函数接收 SerialManager 和 DeviceContext 指针
11. **冻结文件未修改** — SerialManager、FrameParser、其他 Tab、widgets、style_constants.h 均未被修改

## Notes

- PMIC Group 和 Config File Group 的按钮本阶段保持无响应状态
- Slave ID 的 `0x02` 命令下发不在本阶段实现，仅存储值到 DeviceContext
- 错误处理本阶段可以简单处理（qWarning），后续阶段再完善 StatusBar 显示
