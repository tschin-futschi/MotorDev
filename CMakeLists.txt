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
    src/tabs/configtab.cpp
    src/tabs/configtab.h
    src/tabs/registerrwtab.cpp
    src/tabs/registerrwtab.h
    src/tabs/serialdebugtab.cpp
    src/tabs/serialdebugtab.h
    src/tabs/fwflashtab.cpp
    src/tabs/fwflashtab.h
    src/tabs/oscilloscoptab.cpp
    src/tabs/oscilloscoptab.h
    resources/resources.qrc
)

if(WIN32)
    set_target_properties(MotorDev PROPERTIES WIN32_EXECUTABLE FALSE)
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
