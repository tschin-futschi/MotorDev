================================================================
MotorDev Release 打包说明
================================================================

跑过 build_release.bat 之后，build_release\ 目录里已经有：
- MotorDev.exe                   主程序
- Qt6*.dll                       Qt 运行库（windeployqt 自动拷）
- libgcc_s_seh-1.dll             MinGW 运行库
- libstdc++-6.dll                MinGW 运行库
- libwinpthread-1.dll            MinGW 运行库
- platforms\qwindows.dll         Qt 平台插件（必须）
- styles\ / iconengines\ / imageformats\    其它 Qt 插件
- AW86100.dll                    !!! dummy 版本 !!! 必须替换为真实 DLL

下面 4 个文件需要你手工放进 build_release\ 目录，与 MotorDev.exe 同级。


----------------------------------------------------------------
1. 替换 AW86100.dll（真实供应商 DLL）
----------------------------------------------------------------

来源：实验室 / 内网 / 供应商提供
目标路径：build_release\AW86100.dll
替换命令示例：
    copy /Y "<真实 AW86100.dll 路径>" "build_release\AW86100.dll"

注意：
- build_release.bat 跑完后这个位置是 dummy（空实现，会让烧录看起来"成功"
  但实际什么都没做），必须覆盖掉
- 文件名严格 AW86100.dll，大小写一致
- 必须是 x64 版本

判断当前是 dummy 还是真实 DLL 的快速方法：
- 烧录页日志面板有 [DLL] xxx 行   = 真实 DLL
- 只有 [INFO] AwXxx 行，没有 [DLL] = dummy DLL


----------------------------------------------------------------
2. libfftw3-3.dll
----------------------------------------------------------------

来源：FFTW 官方发行版（已在实验室手工管理，不入仓库）
目标路径：build_release\libfftw3-3.dll
替换命令示例：
    copy /Y "<libfftw3-3.dll 路径>" "build_release\libfftw3-3.dll"

为什么需要：真实 AW86100.dll 静态依赖 libfftw3-3.dll；缺这个文件，
MotorDev 启动时 AW86100.dll 也会 load 失败。


----------------------------------------------------------------
3. MSVC / MFC 运行时（共 4 个）
----------------------------------------------------------------

来源：装过 Visual Studio 2015-2022 的机器 C:\Windows\System32\
      或 Microsoft Visual C++ Redistributable 安装目录
目标路径：build_release\ 下并列放 4 个文件

需要的文件：
    mfc140u.dll
    MSVCP140.dll
    VCRUNTIME140.dll
    VCRUNTIME140_1.dll

替换命令示例：
    copy /Y "C:\Windows\System32\mfc140u.dll"      "build_release\"
    copy /Y "C:\Windows\System32\MSVCP140.dll"     "build_release\"
    copy /Y "C:\Windows\System32\VCRUNTIME140.dll" "build_release\"
    copy /Y "C:\Windows\System32\VCRUNTIME140_1.dll" "build_release\"

为什么需要：真实 AW86100.dll 用 MSVC 编译，依赖这套 C/C++ 运行库；
目标机器若没装过 VC Redistributable 就会缺这些 dll，烧录页一加载
AW86100.dll 就报"找不到指定的模块"。

备用方案：在目标机器上装一次 Microsoft Visual C++ Redistributable
        (x64) 安装包，4 个 dll 就齐了，不必随包带。


----------------------------------------------------------------
4. 发布前自检
----------------------------------------------------------------

build_release\ 目录最少要有以下文件：
    MotorDev.exe
    AW86100.dll            (真实版，非 dummy)
    libfftw3-3.dll
    mfc140u.dll
    MSVCP140.dll
    VCRUNTIME140.dll
    VCRUNTIME140_1.dll
    Qt6Core.dll  Qt6Gui.dll  Qt6Widgets.dll  Qt6SerialPort.dll
    Qt6PrintSupport.dll  Qt6OpenGL.dll  Qt6OpenGLWidgets.dll
    Qt6Svg.dll  Qt6SvgWidgets.dll
    libgcc_s_seh-1.dll  libstdc++-6.dll  libwinpthread-1.dll
    platforms\qwindows.dll
    styles\  iconengines\  imageformats\  （目录里有相应 .dll）


----------------------------------------------------------------
5. 打包发给别人
----------------------------------------------------------------

把整个 build_release\ 目录里下面这些东西打成 zip：

    要打包的：
        所有 *.exe / *.dll
        platforms\ / styles\ / iconengines\ / imageformats\ 等子目录

    不要打包的（中间产物，对方用不到）：
        CMakeFiles\
        CMakeCache.txt
        cmake_install.cmake
        Makefile
        MotorDev_autogen\
        stderr.log / stdout.log
        *.obj / *.o

对方拿到 zip 后：
- 解压到任意目录（路径里不要有中文）
- 双击 MotorDev.exe 启动
- 目标机器要求 Windows 10 x64 及以上
- 串口驱动（CH340 / CP2102 / FTDI 等）由对方自己安装

================================================================
