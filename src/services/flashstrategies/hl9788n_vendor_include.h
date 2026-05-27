// =============================================================================
// @file    hl9788n_vendor_include.h
// @brief   包装头：集中包含 vendor Func.h / hl9788n_api_ref.h 并清理宏污染
//
// vendor 头文件污染了若干全局名字（Wait、round 等），与 Qt/Windows/cmath 冲突。
// 本包装头：
//  1) 引入 vendor 两个公共头
//  2) #undef 与系统冲突的宏（Wait → COM 接口、round → cmath::round）
//  3) 在桥接层缺失的 extern 全局函数指针声明
//
// 调用约定：在 cpp 内 #include 此文件时，应放在所有 Qt/std 头文件之后（最后位置），
// 让 #undef 生效到当前 TU 的剩余范围；同时 Qt/std 头先 include 完成自己的展开。
// =============================================================================
#pragma once

// vendor api_ref.h 的 _EXPORT 宏在未定义 HL9788N_OIS_EXPORTS 时取 dllimport 分支
// （生成 __imp_xxx 链接符号）。MotorDev 是静态链入 exe，必须让所有 TU 看到一致
// 的 dllexport 视图，否则链接器报 undefined reference to `__imp_hl9788n_*`。
// vendor cpp 通过 stdafx.h 已经定义该宏；这里为其它调用方（bridge / strategy）
// 显式补一份，确保 #include 进来的 vendor 头看到 dllexport。
#ifndef HL9788N_OIS_EXPORTS
#define HL9788N_OIS_EXPORTS
#endif

// 抑制 vendor 头自身的两类警告：
//   - extra tokens at end of #else directive：api_ref.h:84 处的 `#else if`（应为 `#else`）
//     是 vendor 笔误，GCC 解释为 `#else` 时多余 if 是 token noise，逻辑无影响。
// 仅对 vendor 头的 include 区间生效，不影响后续代码。
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wendif-labels"
#endif

// vendor 公共头
#include "Func.h"
#include "hl9788n_api_ref.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

// -----------------------------------------------------------------------------
// 清理 vendor 宏污染（必须在 vendor 头之后才能 #undef）
// -----------------------------------------------------------------------------
// Wait(a) 与 <objidlbase.h> 的 IEnumXxx::Wait 虚函数冲突
#ifdef Wait
#undef Wait
#endif
// round(fp) 与 <cmath> 的 std::round 冲突
#ifdef round
#undef round
#endif

// -----------------------------------------------------------------------------
// vendor 全局函数指针 extern 声明
// Func.cpp 内定义；api_ref.cpp 自己在文件顶部 extern；其它调用方（桥接层等）
// 需要本声明才能在 attach() 中给它们赋值。
// -----------------------------------------------------------------------------
extern ptrWriteI2CDev WriteI2CDev;
extern ptrReadI2CDev  ReadI2CDev;
extern ptrOutputLog   OutputLog;
