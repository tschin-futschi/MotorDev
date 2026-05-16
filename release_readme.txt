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

下面 6 个文件需要你手工放进 build_release\ 目录（共 3 类），与 MotorDev.exe 同级：
    类 1（1 个）: AW86100.dll（真实供应商版）
    类 2（1 个）: libfftw3-3.dll
    类 3（4 个）: mfc140u.dll / MSVCP140.dll / VCRUNTIME140.dll / VCRUNTIME140_1.dll


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

需要的文件：
    mfc140u.dll
    MSVCP140.dll
    VCRUNTIME140.dll
    VCRUNTIME140_1.dll

为什么需要：真实 AW86100.dll 用 MSVC 编译，依赖这套 C/C++ 运行库；
目标机器若没装过 VC Redistributable 就会缺这些 dll，烧录页一加载
AW86100.dll 就报"找不到指定的模块"。

获取方式有 3 种，按推荐顺序：

【方法 A 推荐】在目标机器装一次微软官方运行库（一次性）

    下载："Microsoft Visual C++ Redistributable for Visual Studio
           2015-2022 (x64)"
    地址：https://aka.ms/vs/17/release/vc_redist.x64.exe
    装完后 4 个 dll 自动落到 C:\Windows\System32\

    走方法 A 时这 4 个 dll 不用随 zip 发，只发安装包 + zip 即可。

【方法 B】从自己机器的 System32 拷过来（你装过 VS 或 VC Redist）

    先确认存在：
        dir C:\Windows\System32\mfc140u.dll
        dir C:\Windows\System32\msvcp140.dll
        dir C:\Windows\System32\vcruntime140.dll
        dir C:\Windows\System32\vcruntime140_1.dll
    任一报"找不到"说明本机也没装过 → 改走方法 A

    拷贝命令：
        copy /Y "C:\Windows\System32\mfc140u.dll"        "build_release\"
        copy /Y "C:\Windows\System32\msvcp140.dll"       "build_release\"
        copy /Y "C:\Windows\System32\vcruntime140.dll"   "build_release\"
        copy /Y "C:\Windows\System32\vcruntime140_1.dll" "build_release\"

【方法 C】从 Visual Studio 安装目录的 Redist 子目录拷

    路径模板（<Edition> = Community / Professional / Enterprise,
              <version> = 14.xx 子版本号）：
        C:\Program Files\Microsoft Visual Studio\2022\<Edition>\VC\Redist\MSVC\<version>\x64\Microsoft.VC143.CRT\
        C:\Program Files\Microsoft Visual Studio\2022\<Edition>\VC\Redist\MSVC\<version>\x64\Microsoft.VC143.MFC\

    .CRT\ 里有 MSVCP140.dll / VCRUNTIME140.dll / VCRUNTIME140_1.dll
    .MFC\ 里有 mfc140u.dll


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


----------------------------------------------------------------
6. !!! 必做 !!! 调整 USB-Serial Latency Timer 为 1 ms
----------------------------------------------------------------

新机器第一次烧录前必须改这个 Windows 驱动设置；不改的话烧录大概率
会卡在 AwBootcontrol 阶段失败，日志面板报：
    [DLL] BootControl fail with RET_BOOTCONTROL_STOP_BOOTLOADER
    [DLL] Flash Hand Compare Check Failed!
    AwBootcontrol failed (return -23)

根因：Windows 默认 USB-Serial Latency Timer = 16 ms，导致每次 I2C
透传往返耗时 15-17 ms；OIS IC 在 boot 模式握手序列对命令间隔有窗口
约束，间隔太大 IC 会自动 STOP_BOOTLOADER 退回正常模式，握手失败。
改到 1 ms 后单次透传降到 1-2 ms，握手能在窗口内完成。

操作步骤：

  1. 按 Win + X，打开 设备管理器
  2. 展开 "端口（COM 和 LPT）"
  3. 找到正在用的 USB-Serial 设备
     例：USB Serial Port (COMx) / CH340 (COMx) / Silicon Labs CP210x (COMx)
  4. 右键 → 属性
  5. 切到 Port Settings 选项卡（中文系统是"端口设置"）
  6. 点 Advanced... 按钮（中文系统是"高级"）
  7. 找到 Latency Timer (msec) 下拉框（中文系统是"延迟计时器"）
  8. 默认 16，改成 1
  9. 确定 → 退出所有对话框
  10. 拔出 USB 重新插入，让设置生效

  快速验证：MotorDev 烧录页烧一次，日志面板里 [I2C-W TIMING] / [I2C-R
  TIMING] 应该是 us 级（几百到几千 us），不应该是 15000+ us。
  [FP-DIAG] 里的 execMs 应该是 0-2，不应该是 13-17。

  注意：这个设置是按"该 USB-Serial 设备 + 当前 Windows 用户"持久化的，
  换 USB-Serial 设备 / 换电脑 / 换用户 都要重新改一次。

================================================================
