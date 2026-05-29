// =============================================================================
// @file    stdafx.h
// @brief   MotorDev minimal stdafx forwarder for DW9786 vendor sources.
//
// vendor 的 Func.cpp / dw9786_api_ref.cpp 头部 #include "StdAfx.h" 是 MSVC 旧式
// precompiled header 习惯。MotorDev 项目不使用 PCH，因此提供这个最小转发头
// 集中包含 vendor 隐式依赖的 Windows 头文件，避免修改 vendor 源码。
//
// 与 hl9788n/stdafx.h 完全对称，仅 DW9786_OIS_EXPORTS 区别。
//
// 注意：本文件由 MotorDev 工程引入，不是 Dongwoon 原厂文件。
// =============================================================================
#pragma once

// Enable C11 Annex K bounds-checked functions (fopen_s 等) before any CRT include
#ifndef __STDC_WANT_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__ 1
#endif

// vendor 源码使用 _T(__FUNCTION__) 这类 MSVC-特有写法。在 UNICODE 模式下 _T()
// 展开为 L 前缀（如 L"foo"），但 __FUNCTION__ 在 MinGW 是运行期 const char*，
// 不是字符串字面量，无法 L 拼接 → 编译错误 "L__FUNCTION__" not declared。
// 解决：在包含 <tchar.h> 之前 undef UNICODE/_UNICODE，使 TCHAR=char、_T()
// 展开为空 → vendor 走 ANSI 路径，与 MotorDev 的 char 字符串日志接口天然对齐。
#ifdef UNICODE
#undef UNICODE
#endif
#ifdef _UNICODE
#undef _UNICODE
#endif

// vendor 头使用 _EXPORT = __declspec(dllimport/export) 区分 DLL 客户端/导出端；
// MotorDev 将 vendor 静态链入 exe，故定义 DW9786_OIS_EXPORTS 让 _EXPORT 取
// dllexport 分支，避免 cpp 内函数定义与 .h 内 dllimport 声明的不一致告警。
#ifndef DW9786_OIS_EXPORTS
#define DW9786_OIS_EXPORTS
#endif

// Func.h（若已被先于 stdafx.h 包含，例如 Func.cpp 中）定义了宏 Wait(a)，
// 与 <objidlbase.h> 中 IEnumXxx::Wait 虚函数声明冲突。Wait 宏是 vendor 死代码
// （烧录路径不用），undef 后不影响 vendor 功能。
#ifdef Wait
#undef Wait
#endif

#include <windows.h>

// 防御性二次 undef
#ifdef UNICODE
#undef UNICODE
#endif
#ifdef _UNICODE
#undef _UNICODE
#endif

#include <tchar.h>

// 提前 include <math.h>：vendor dw9786_api_ref.h:52 有 `#define round(fp) ...`
// 会污染后续 <math.h>/<cmath> 中 `double round(double)` 声明。在 vendor
// 头之前先把 math.h 解析完，round 即作为函数已知；vendor 后续 #define round
// 仅影响 vendor 内部代码，不再破坏 std 头。
#include <math.h>

// 强制覆盖 _T / _TEXT / TEXT 宏为 no-op
#ifdef _T
#undef _T
#endif
#define _T(x) x

#ifdef _TEXT
#undef _TEXT
#endif
#define _TEXT(x) x

#ifdef TEXT
#undef TEXT
#endif
#define TEXT(x) x

#include <stdio.h>
#include <stdlib.h>

// MinGW compatibility shims for MSVC-only safe-CRT helpers used by vendor sources.
#if !defined(_MSC_VER)
    #ifndef _countof
        #define _countof(arr) (sizeof(arr) / sizeof((arr)[0]))
    #endif

    #ifndef _vstprintf_s
        #define _vstprintf_s(buf, size, fmt, va) _vsntprintf((buf), (size), (fmt), (va))
    #endif
#endif
