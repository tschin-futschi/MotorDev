cmake_minimum_required(VERSION 3.16)

project(MotorDev VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg SvgWidgets SerialPort PrintSupport OpenGLWidgets)

qt_add_executable(MotorDev
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/devicecontext.cpp
    src/devicecontext.h
    src/frameparser.cpp
    src/frameparser.h
    src/serialmanager.cpp
    src/serialmanager.h
    src/models/scopechannelmodel.cpp
    src/models/scopechannelmodel.h
    src/models/channelbuffer.cpp
    src/models/channelbuffer.h
    src/protocol/register_utils.cpp
    src/protocol/register_utils.h
    src/protocol/sampling_config.cpp
    src/protocol/sampling_config.h
    src/protocol/firmware_parser.cpp
    src/protocol/firmware_parser.h
    src/protocol/batch_register_file.cpp
    src/protocol/batch_register_file.h
    src/services/appconfigservice.cpp
    src/services/appconfigservice.h
    src/services/configservice.cpp
    src/services/configservice.h
    src/services/commanddispatcher.cpp
    src/services/commanddispatcher.h
    src/services/cyclicwriteservice.cpp
    src/services/cyclicwriteservice.h
    src/services/dw9786oisresetservice.cpp
    src/services/dw9786oisresetservice.h
    src/services/generatorservice.cpp
    src/services/generatorservice.h
    src/services/registerservice.cpp
    src/services/registerservice.h
    src/services/batchregisterservice.cpp
    src/services/batchregisterservice.h
    src/services/blockreadservice.cpp
    src/services/blockreadservice.h
    src/services/scopeservice.cpp
    src/services/scopeservice.h
    src/services/scoperecordservice.cpp
    src/services/scoperecordservice.h
    src/services/simulatorserial.cpp
    src/services/simulatorserial.h
    src/services/simulatorservice.cpp
    src/services/simulatorservice.h
    src/services/flashstrategy.cpp
    src/services/flashstrategy.h
    src/services/flashstrategyregistry.cpp
    src/services/flashstrategyregistry.h
    src/services/fwflashservice.cpp
    src/services/fwflashservice.h
    src/services/flashstoreservice.cpp
    src/services/flashstoreservice.h
    src/services/flashstrategies/aw_local_isp_strategy.cpp
    src/services/flashstrategies/aw_local_isp_strategy.h
    src/services/flashstrategies/aw86008_strategy.cpp
    src/services/flashstrategies/aw86008_strategy.h
    src/services/flashstrategies/aw86100_strategy.cpp
    src/services/flashstrategies/aw86100_strategy.h
    src/services/flashstrategies/dw9786_strategy.cpp
    src/services/flashstrategies/dw9786_strategy.h
    src/services/flashstrategies/dw9788_strategy.cpp
    src/services/flashstrategies/dw9788_strategy.h
    src/services/flashstrategies/hl9788n_bridge.cpp
    src/services/flashstrategies/hl9788n_bridge.h
    src/services/flashstrategies/hl9788n_vendor_include.h
    src/services/flashstrategies/vendor/hl9788n/Func.cpp
    src/services/flashstrategies/vendor/hl9788n/Func.h
    src/services/flashstrategies/vendor/hl9788n/hl9788n_api_ref.cpp
    src/services/flashstrategies/vendor/hl9788n/hl9788n_api_ref.h
    src/services/flashstrategies/vendor/hl9788n/stdafx.h
    src/services/flashstrategies/dw9786_bridge.cpp
    src/services/flashstrategies/dw9786_bridge.h
    src/services/flashstrategies/dw9786_vendor_include.h
    src/services/flashstrategies/vendor/dw9786/Func.cpp
    src/services/flashstrategies/vendor/dw9786/Func.h
    src/services/flashstrategies/vendor/dw9786/dw9786_api_ref.cpp
    src/services/flashstrategies/vendor/dw9786/dw9786_api_ref.h
    src/services/flashstrategies/vendor/dw9786/dw9786_symbol_rename.h
    src/services/flashstrategies/vendor/dw9786/stdafx.h
    src/protocol/motor_protocol.cpp
    src/protocol/motor_protocol.h
    src/ui/style_constants.h
    src/widgets/aboutdialog.cpp
    src/widgets/aboutdialog.h
    src/widgets/activitybar.cpp
    src/widgets/activitybar.h
    src/widgets/logpanel.cpp
    src/widgets/logpanel.h
    src/widgets/registertable.cpp
    src/widgets/registertable.h
    src/widgets/sidebar.cpp
    src/widgets/sidebar.h
    src/widgets/scopebottompanel.cpp
    src/widgets/scopebottompanel.h
    src/widgets/scopechannelstrip.cpp
    src/widgets/scopechannelstrip.h
    src/widgets/scopegeneratorpanel.cpp
    src/widgets/scopegeneratorpanel.h
    src/widgets/scopemarqueelabel.cpp
    src/widgets/scopemarqueelabel.h
    src/widgets/scopeplotwidget.cpp
    src/widgets/scopeplotwidget.h
    src/widgets/scopepreviewwidget.cpp
    src/widgets/scopepreviewwidget.h
    src/widgets/scoperegisterpanel.cpp
    src/widgets/scoperegisterpanel.h
    src/widgets/scopestylepanel.cpp
    src/widgets/scopestylepanel.h
    src/widgets/topbar.cpp
    src/widgets/topbar.h
    src/widgets/fwfileinfopanel.cpp
    src/widgets/fwfileinfopanel.h
    src/widgets/fwflashcontrolpanel.cpp
    src/widgets/fwflashcontrolpanel.h
    src/widgets/fwflashlogpanel.cpp
    src/widgets/fwflashlogpanel.h
    src/tabs/configtab.cpp
    src/tabs/configtab.h
    src/tabs/registerrwtab.cpp
    src/tabs/registerrwtab.h
    src/tabs/serialdebugtab.cpp
    src/tabs/serialdebugtab.h
    src/tabs/fwflashtab.cpp
    src/tabs/fwflashtab.h
    src/tabs/flashstoragetab.cpp
    src/tabs/flashstoragetab.h
    src/tabs/oscilloscoptab.cpp
    src/tabs/oscilloscoptab.h
    resources/resources.qrc
    translations/translations.qrc
)

if(WIN32)
    set_target_properties(MotorDev PROPERTIES WIN32_EXECUTABLE FALSE)
    # motordev_manifest.rc 引用 motordev.manifest，声明 PerMonitorV2 DPI awareness。
    # 必须显式接入，否则 Qt 进程在锁屏-解锁等边界条件下可能拿到不稳定的 system DPI，
    # 导致 QSS/字体按错误的 scale 渲染（参见 plan_dpi_lockscreen_font_fix）。
    target_sources(MotorDev PRIVATE
        resources/app_icon.rc
        resources/motordev_manifest.rc
    )
    if(MSVC)
        # 禁止 MSVC 链接器自动生成默认 manifest，由 motordev_manifest.rc 显式提供。
        target_link_options(MotorDev PRIVATE "/MANIFEST:NO")
    endif()
endif()

target_include_directories(MotorDev PRIVATE
    src
    src/services/flashstrategies/vendor/hl9788n
    src/services/flashstrategies/vendor/dw9786
)

target_link_libraries(MotorDev PRIVATE
    Qt6::Widgets
    Qt6::Svg
    Qt6::SvgWidgets
    Qt6::SerialPort
    Qt6::PrintSupport
    Qt6::OpenGLWidgets
)

if(WIN32)
    # HL9788N vendor 库依赖：m_delay() 用 timeBeginPeriod/timeGetTime；
    # hl978x_extfuncinit() 用 GetFileVersionInfo。
    target_link_libraries(MotorDev PRIVATE winmm version)

    # HL9788N 与 DW9786 vendor 来自同一厂商 (Dongwoon)，两组 cpp 共有十几处同名
    # 全局符号。已通过预处理器 #define 在 vendor/dw9786/dw9786_symbol_rename.h
    # 把所有 dw9786 vendor 内部符号 rename 为 dw9786_v_* 前缀，让两个 vendor 完全
    # 独立。link 阶段无冲突，不再需要 --allow-multiple-definition。

    # vendor 源码使用 _T(__FUNCTION__) 等 MSVC-特有写法，需 ANSI 路径才能编译。
    # Qt6 在 Windows 默认全局定义 UNICODE/_UNICODE，会破坏 vendor 编译。
    # 这里只对 vendor 两个 cpp 取消这两个宏（其它源码不变）。
    # 同时抑制 vendor 自身的两类原有警告（不修 vendor 文件）：
    #   -Wno-write-strings : vendor log_tprintf 的 TCHAR* 非 const 形参 + 我们把 _T(x)
    #                        重定义为 x 后，string literal → char* 在 C++ 被禁。仅警告，
    #                        vendor 内部只读不写格式串。
    #   -Wno-endif-labels  : vendor api_ref.h:84 处 `#else if`（应为 `#else`）的 vendor bug。
    #                        GCC 解释为 #else 时多余 if 是 token noise，逻辑无影响。
    set_source_files_properties(
        src/services/flashstrategies/vendor/hl9788n/Func.cpp
        src/services/flashstrategies/vendor/hl9788n/hl9788n_api_ref.cpp
        src/services/flashstrategies/vendor/dw9786/Func.cpp
        src/services/flashstrategies/vendor/dw9786/dw9786_api_ref.cpp
        PROPERTIES COMPILE_OPTIONS "-UUNICODE;-U_UNICODE;-Wno-write-strings;-Wno-endif-labels"
    )
endif()

# （历史）AW86100.dll POST_BUILD 部署已于 2026-05-19 协议 v2.6 / v2.7 改版后移除：
# 烧录改走 STM32 本地 ISP（协议 0x32~0x37），上位机不再依赖 PC 端 DLL。
# 物理 DLL 文件保留在 ThirdParty/FlashLoad/AW86100/x64/，便于回滚时快速恢复。
