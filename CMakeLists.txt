cmake_minimum_required(VERSION 3.16)

project(MotorDev VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg SvgWidgets SerialPort PrintSupport)

qt_add_executable(MotorDev
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/ui/style_constants.h
    src/widgets/activitybar.cpp
    src/widgets/activitybar.h
    src/widgets/sidebar.cpp
    src/widgets/sidebar.h
    src/widgets/topbar.cpp
    src/widgets/topbar.h
    src/tabs/configtab.cpp
    src/tabs/configtab.h
    src/tabs/registerrwtab.cpp
    src/tabs/registerrwtab.h
    src/tabs/fwflashtab.cpp
    src/tabs/fwflashtab.h
    src/tabs/oscilloscoptab.cpp
    src/tabs/oscilloscoptab.h
    resources/resources.qrc
)

target_include_directories(MotorDev PRIVATE src)

target_link_libraries(MotorDev PRIVATE
    Qt6::Widgets
    Qt6::Svg
    Qt6::SvgWidgets
    Qt6::SerialPort
    Qt6::PrintSupport
)
