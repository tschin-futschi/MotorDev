// =============================================================================
// @file    dw9786_vendor_include.h
// @brief   包装头：集中包含 vendor Func.h / dw9786_api_ref.h 并清理宏污染
//
// vendor 头文件污染了若干全局名字（Wait、round 等），与 Qt/Windows/cmath 冲突。
// 本包装头：
//  1) 引入 vendor 两个公共头
//  2) #undef 与系统冲突的宏（Wait → COM 接口、round → cmath::round）
//  3) 在桥接层缺失的 extern 全局函数指针声明
//
// 调用约定：在 cpp 内 #include 此文件时，应放在所有 Qt/std 头文件之后（最后位置），
// 让 #undef 生效到当前 TU 的剩余范围；同时 Qt/std 头先 include 完成自己的展开。
//
// 与 hl9788n_vendor_include.h 完全对称。
// =============================================================================
#pragma once

// [MOTORDEV PATCH] Rename dw9786 vendor internal symbols so bridge/strategy TUs
// see the same renamed identifiers as vendor TUs (must be first include).
#include "dw9786_symbol_rename.h"

// vendor api_ref.h 的 _EXPORT 宏在未定义 DW9786_OIS_EXPORTS 时取 dllimport 分支
// （生成 __imp_xxx 链接符号）。MotorDev 是静态链入 exe，必须让所有 TU 看到一致
// 的 dllexport 视图，否则链接器报 undefined reference to `__imp_dw9786_*`。
// vendor cpp 通过 stdafx.h 已经定义该宏；这里为其它调用方（bridge / strategy）
// 显式补一份，确保 #include 进来的 vendor 头看到 dllexport。
#ifndef DW9786_OIS_EXPORTS
#define DW9786_OIS_EXPORTS
#endif

// 抑制 vendor 头自身的两类警告：
//   - extra tokens at end of #else directive：dw9786_api_ref.h:60 处的 `#else if`
//     （应为 `#else`）是 vendor 笔误。
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wendif-labels"
#endif

// vendor 公共头
#include "Func.h"
#include "dw9786_api_ref.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

// -----------------------------------------------------------------------------
// 清理 vendor 宏污染（必须在 vendor 头之后才能 #undef）
// -----------------------------------------------------------------------------
#ifdef Wait
#undef Wait
#endif
#ifdef round
#undef round
#endif

// -----------------------------------------------------------------------------
// vendor 全局函数指针 extern 声明
// Func.cpp 内定义；其它调用方（桥接层等）需要本声明才能在 attach() 中给它们
// 赋值。
// -----------------------------------------------------------------------------
extern ptrWriteI2CDev WriteI2CDev;
extern ptrReadI2CDev  ReadI2CDev;
extern ptrOutputLog   OutputLog;
