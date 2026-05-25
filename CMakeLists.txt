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
    src/services/configservice.cpp
    src/services/configservice.h
    src/services/commanddispatcher.cpp
    src/services/commanddispatcher.h
    src/services/cyclicwriteservice.cpp
    src/services/cyclicwriteservice.h
    src/services/generatorservice.cpp
    src/services/generatorservice.h
    src/services/registerservice.cpp
    src/services/registerservice.h
    src/services/scopeservice.cpp
    src/services/scopeservice.h
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
    src/services/flashstrategies/aw86006_strategy.cpp
    src/services/flashstrategies/aw86006_strategy.h
    src/services/flashstrategies/aw86100_strategy.cpp
    src/services/flashstrategies/aw86100_strategy.h
    src/services/flashstrategies/dw9786_strategy.cpp
    src/services/flashstrategies/dw9786_strategy.h
    src/services/flashstrategies/dw9788_strategy.cpp
    src/services/flashstrategies/dw9788_strategy.h
    src/protocol/motor_protocol.cpp
    src/protocol/motor_protocol.h
    src/ui/style_constants.h
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

target_include_directories(MotorDev PRIVATE src)

target_link_libraries(MotorDev PRIVATE
    Qt6::Widgets
    Qt6::Svg
    Qt6::SvgWidgets
    Qt6::SerialPort
    Qt6::PrintSupport
    Qt6::OpenGLWidgets
)

# （历史）AW86100.dll POST_BUILD 部署已于 2026-05-19 协议 v2.6 / v2.7 改版后移除：
# 烧录改走 STM32 本地 ISP（协议 0x32~0x37），上位机不再依赖 PC 端 DLL。
# 物理 DLL 文件保留在 ThirdParty/FlashLoad/AW86100/x64/，便于回滚时快速恢复。
